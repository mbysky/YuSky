// ConvetPngTo2N.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include "png.h"

#define TFC_DEBUG(x)

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
	u8* pixelData;
	int imageWidth;
	int imageHeight;
	int imageBitDepth;
}ImageInfo;

typedef struct {
	u8* data;
	int size;
	int offset;
}ImageSource;

//----------------------------------------------------------------------------------------------------------------
static void pngReaderCallback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	ImageSource* isource = (ImageSource*)png_get_io_ptr(png_ptr);

	if(isource->offset + length <= isource->size)
	{
		memcpy(data, isource->data + isource->offset, length);
		isource->offset += length;
	}
	else
	{
		png_error(png_ptr,"pngReaderCallback failed");
	}
}

//----------------------------------------------------------------------------------------------------------------

ImageInfo*  decodePNGFromStream(const u8* pixelData, const u32& dataSize)
{
	png_structp png_ptr;
	png_infop info_ptr;
	int width, height, rowBytes;
	png_byte color_type;  //¿ÉÒÔÊÇPNG_COLOR_TYPE_RGB,PNG_COLOR_TYPE_PALETTE.......
	png_byte bit_depth;
	png_colorp palette;

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,NULL,NULL);
	if (!png_ptr)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		TFC_DEBUG("ReadPngFile: Failed to create png_ptr");
	}
	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		TFC_DEBUG("ReadPngFile: Failed to create info_ptr");
	}
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		TFC_DEBUG("ReadPngFile: Failed to read the PNG file");
	}


	ImageSource imgsource;
	imgsource.data = (u8*)pixelData;
	imgsource.size = dataSize;
	imgsource.offset = 0;
	//define our own callback function for I/O instead of reading from a file
	png_set_read_fn(png_ptr,&imgsource, pngReaderCallback );


	/* **************************************************
	* The low-level read interface in libpng (http://www.libpng.org/pub/png/libpng-1.2.5-manual.html)
	* **************************************************
	*/
	png_read_info(png_ptr, info_ptr);
	width = info_ptr->width;
	height = info_ptr->height;
	color_type = info_ptr->color_type;
	bit_depth = info_ptr->bit_depth;
	rowBytes = info_ptr->rowbytes;
	palette= info_ptr->palette;

	// Convert stuff to appropriate formats!
	if(color_type==PNG_COLOR_TYPE_PALETTE)
	{
		png_set_packing(png_ptr);
		png_set_palette_to_rgb(png_ptr); //Expand data to 24-bit RGB or 32-bit RGBA if alpha available.
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_gray_1_2_4_to_8(png_ptr);
	if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);
	if (bit_depth == 16)
		png_set_strip_16(png_ptr);


	//Expand paletted or RGB images with transparency to full alpha channels so the data will be available as RGBA quartets.
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
	{
		png_set_tRNS_to_alpha(png_ptr);
	}

	//png_read_update_info(png_ptr, info_ptr);
	u8* rgba = new u8[width * height * 4];  //each pixel(RGBA) has 4 bytes
	png_bytep * row_pointers;
	row_pointers = (png_bytep*)png_malloc((png_structp)png_ptr, sizeof(png_bytep) * height);
	for (int y = 0; y < height; y++)
	{
		row_pointers[y] = (png_bytep)png_malloc((png_structp)png_ptr, width << 2); //each pixel(RGBA) has 4 bytes
	}
	png_read_image(png_ptr, row_pointers);

	//unlike store the pixel data from top-left corner, store them from bottom-left corner for OGLES Texture drawing...
	int pos = (width * height * 4) - (4 * width);
	for(int row = 0; row < height; row++)
	{
		for(int col = 0; col < (4 * width); col += 4)
		{
			rgba[pos++] = row_pointers[row][col];        // red
			rgba[pos++] = row_pointers[row][col + 1];    // green
			rgba[pos++] = row_pointers[row][col + 2];    // blue
			rgba[pos++] = row_pointers[row][col + 3];    // alpha
		}
		pos=(pos - (width * 4)*2); //move the pointer back two rows
	}

	ImageInfo* imageInfo = (ImageInfo*)malloc(sizeof(ImageInfo));
	imageInfo->pixelData = rgba;
	imageInfo->imageHeight = height;
	imageInfo->imageWidth = width;

	//clean up after the read, and free any memory allocated
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	return imageInfo;
}

//----------------------------------------------------------------------------------------------------------------

ImageInfo*   decodePNGFromFile(char* fileName)
{
	char png_header[8];
	png_structp png_ptr;
	png_infop info_ptr;
	png_uint_32 width, height, rowBytes;
	int color_type; 
	int bit_depth;
	int interlace_type;
	png_colorp palette; 

	/* open file and test for it being a png */
	FILE *file = fopen(fileName, "rb");
	fread(png_header, 1, 8, file);
	if(png_sig_cmp((png_bytep)png_header, 0, 8))
	{
		TFC_DEBUG("Not a PNG file...");
		fclose(file);
	}
	/* initialise structures for reading a png file */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	info_ptr = png_create_info_struct(png_ptr);
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, png_infopp_NULL);
		TFC_DEBUG("ReadPngFile: Failed to read the PNG file");
		fclose(file);
	}
	//I/O initialisation methods
	png_init_io(png_ptr, file);
	png_set_sig_bytes(png_ptr, 8);  //Required!!!


	//png_read_info(png_ptr, info_ptr);

	/* **************************************************
	* The high-level read interface in libpng (http://www.libpng.org/pub/png/libpng-1.2.5-manual.html)
	* **************************************************
	*/
	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
		&interlace_type, int_p_NULL, int_p_NULL);

	int iCharSize = 0;
	if (PNG_COLOR_TYPE_RGB == color_type)
	{
		iCharSize = 3;
	}
	else if (PNG_COLOR_TYPE_RGB_ALPHA == color_type)
	{
		iCharSize = 4;
	}
	else if (PNG_COLOR_TYPE_PALETTE == color_type)
	{
		iCharSize = 1;
	}
	else
	{
		// error
	}

	unsigned char* rgba = new unsigned char[width * height * iCharSize];  //each pixel(RGBA) has 4 bytes
	png_bytep* row_pointers = png_get_rows(png_ptr, info_ptr);

	//Original PNG pixel data stored from top-left corner, BUT OGLES Texture drawing is from bottom-left corner
	//int pos = 0;
	//for(int row = 0; row < height; row++)
	//{
	//	for(int col = 0; col < (4 * width); col += 4)
	//	{
	//		rgba[pos++] = row_pointers[row][col];     // red
	//		rgba[pos++] = row_pointers[row][col + 1]; // green
	//		rgba[pos++] = row_pointers[row][col + 2]; // blue
	//		rgba[pos++] = row_pointers[row][col + 3]; // alpha
	//	}
	//}


	////unlike store the pixel data from top-left corner, store them from bottom-left corner for OGLES Texture drawing...
	//int pos = (width * height * 4) - (4 * width);
	//for(int row = 0; row < height; row++)
	//{
	//	for(int col = 0; col < (4 * width); col += 4)
	//	{
	//		rgba[pos++] = row_pointers[row][col];        // red
	//		rgba[pos++] = row_pointers[row][col + 1]; // green
	//		rgba[pos++] = row_pointers[row][col + 2]; // blue
	//		rgba[pos++] = row_pointers[row][col + 3]; // alpha
	//	}
	//	pos=(pos - (width * 4) * 2); //move the pointer back two rows
	//}

	int pos = 0;
	for(int row = 0; row < height; row++)
	{
		for(int col = 0; col < width * iCharSize; ++ col)
		{
			rgba[pos++] = row_pointers[row][col];
		}
	}

	ImageInfo* imageInfo = (ImageInfo*)malloc(sizeof(ImageInfo));
	imageInfo->pixelData = rgba;
	imageInfo->imageHeight = height;
	imageInfo->imageWidth = width;
	imageInfo->imageBitDepth = iCharSize * 8;

	//clean up after the read, and free any memory allocated
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	fclose(file);
	return imageInfo;
}

//----------------------------------------------------------------------------------------------------------------

int createPNGTextureFromStream(const u8* pixelData, const u32& dataSize)
{
	//GLuint textureID;
	//glEnable(GL_TEXTURE_2D);
	//glGenTextures(1,&textureID);
	//glBindTexture(GL_TEXTURE_2D,textureID);
	//ImageInfo* imageInfo = decodePNGFromStream(pixelData, dataSize);

	//glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,imageInfo->imageWidth,imageInfo->imageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageInfo->pixelData);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);

	//delete[] imageInfo->pixelData;

	//delete imageInfo;
	//return textureID;
	return 0;
}

#define ERROR -1

static void pngSaveCallback(png_structp png_ptr, png_bytep data, png_size_t length)
{
	FILE * fp = (FILE*)png_get_io_ptr(png_ptr);
	fwrite(data, length , 1, fp);
	//ImageSource* isource = (ImageSource*)png_get_io_ptr(png_ptr);

	//if(isource->offset + length <= isource->size)
	//{
	//	memcpy(isource->data + isource->offset, data, length);
	//	isource->offset += length;
	//}
	//else
	//{
	//	png_error(png_ptr,"pngSaveCallback failed");
	//}
}

//////////////////////////////////////////////////////////////////////////
int SavePNGToFile(char* szFileName, ImageInfo * pImageInfo)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_colorp palette;

	/* Open the file */
	fp = fopen(szFileName, "wb");
	if (fp == NULL)
		return (ERROR);

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL)
	{
		fclose(fp);
		return (ERROR);
	}
	/* Allocate/initialize the image information data.  REQUIRED */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL)
	{
		fclose(fp);
		png_destroy_write_struct(&png_ptr,  png_infopp_NULL);
		return (ERROR);
	}
	/* Set error handling.  REQUIRED if you aren't supplying your own
	* error handling functions in the png_create_write_struct() call.
	*/
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		/* If we get here, we had a problem writing the file */
		fclose(fp);
		png_destroy_write_struct(&png_ptr, &info_ptr);
		return (ERROR);
	}

	png_init_io(png_ptr, fp);

	//png_set_sig_bytes(png_ptr, 8);  //Required!!!

	// don't support palette
	int color_type = 0;
	if (pImageInfo->imageBitDepth == 24)
	{
		color_type = PNG_COLOR_TYPE_RGB;
	}
	else if (pImageInfo->imageBitDepth == 32)
	{
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	}
	png_set_IHDR(png_ptr, info_ptr, pImageInfo->imageWidth, pImageInfo->imageHeight, 8, color_type,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	//palette = (png_colorp)png_malloc(png_ptr, PNG_MAX_PALETTE_LENGTH * png_sizeof(png_color));
	//png_set_PLTE(png_ptr, info_ptr, palette, PNG_MAX_PALETTE_LENGTH);

	png_set_write_fn(png_ptr, fp, pngSaveCallback, NULL);

	png_write_info(png_ptr, info_ptr);

	png_set_packing(png_ptr);

	png_bytep * row_pointers = (png_bytep *)malloc(pImageInfo->imageHeight * sizeof(png_bytep));

	const int kiCharSize = pImageInfo->imageBitDepth / 8;
	for (int i = 0; i < pImageInfo->imageHeight; ++ i)
	{
		//row_pointers[i] = (png_bytep)pImageInfo->pixelData + i * pImageInfo->imageWidth * kiCharSize;
		row_pointers[i] = (png_bytep)pImageInfo->pixelData + i * pImageInfo->imageWidth * kiCharSize;
	}

	png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

	//png_write_image(png_ptr, row_pointers);

	//png_write_end(png_ptr, info_ptr);

	//png_free(png_ptr, palette);
	//palette = NULL;

	//clean up after the read, and free any memory allocated
	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(row_pointers);
	row_pointers = NULL;

	fclose(fp);
	return 1;
}
//////////////////////////////////////////////////////////////////////////

static u32 GetAdjustToPowOfTwoDimension( u32 n ) 
{
	if( n == 0 ) return 0;
	if( n == 1 ) return 1;
	if( n <= 2 ) return 2;
	if( n <= 4 ) return 4;
	if( n <= 8 ) return 8;
	if( n <= 16 ) return 16;
	if( n <= 32 ) return 32;
	if( n <= 64 ) return 64;
	if( n <= 128 ) return 128;
	if( n <= 256 ) return 256;
	if( n <= 512 ) return 512;
	if( n <= 1024 ) return 1024;
	if( n <= 2048 ) return 2048;
	if( n <= 4096 ) return 4096;
/*	if( n <= 8192 ) return 8192;
	if( n <= 16384 ) return 16384;
	if( n <= 32768 ) return 32768;
	if( n <= 65536 ) return 65536; */
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{

	//FILE *fp = NULL;
	//fp = fopen("d:\\pngtest.png", "rb");
	//if (fp == NULL)
	//{
	//	printf("can't open file !");
	//	return 1;
	//}
	//fseek(fp, 0, SEEK_END);
	//int iSize = ftell(fp);
	//fclose(fp);
	//int texId = createPNGTextureFromStream(pixelData, dataSize);
	char* fileName = "example.png";
	//int texId = createPNGTextureFromFile(fileName);

	ImageInfo * testImage = decodePNGFromFile(argv[1]);
	
	ImageInfo * pTestOutImage = new ImageInfo();
	const int kiCharSize = testImage->imageBitDepth / 8;
	pTestOutImage->imageBitDepth = testImage->imageBitDepth;
	pTestOutImage->imageHeight = GetAdjustToPowOfTwoDimension(testImage->imageHeight);
	pTestOutImage->imageWidth = GetAdjustToPowOfTwoDimension(testImage->imageWidth);
	if (pTestOutImage->imageWidth < pTestOutImage->imageHeight)
	{
		pTestOutImage->imageWidth = pTestOutImage->imageHeight;
	}
	else
	{
		pTestOutImage->imageHeight = pTestOutImage->imageWidth;
	}
	pTestOutImage->pixelData = new u8[pTestOutImage->imageWidth * pTestOutImage->imageHeight * kiCharSize];
	memset(pTestOutImage->pixelData, 0, pTestOutImage->imageWidth * pTestOutImage->imageHeight * kiCharSize);
	for (int i = 0; i < testImage->imageHeight; ++ i)
	{
		u8 * pDst = pTestOutImage->pixelData + (pTestOutImage->imageHeight - testImage->imageHeight + i) * pTestOutImage->imageWidth * kiCharSize;
		//u8 * pSrc = testImage->pixelData + (testImage->imageHeight - i - 1) * testImage->imageWidth * kiCharSize;
		u8 * pSrc = testImage->pixelData + i * testImage->imageWidth * kiCharSize;
		memcpy(pDst, pSrc, testImage->imageWidth * kiCharSize);
	}

	char szOutFilename[256] = {0};
	SavePNGToFile(argv[1], pTestOutImage);
	//SavePNGToFile("D:\\asdfasdf.png", pTestOutImage);

	return 0;
}

