#define _CRT_SECURE_NO_WARNINGS
#define PRINT_ERRORS 1
#define PRINT_DEBUG 0
#define PRINT_INFO 1

#include "steganography.h"

//Marks the file as containing hidden bytes
uint8_t _STEG_PAYLOAD_START[] = { 0x70, 0x65, 0x65, 0x70, 0x6f, 0x73, 0x74, 0x61, 0x72, 0x74 };

//Marks the end of the payload
uint8_t _STEG_PAYLOAD_END[] = { 0x70, 0x65, 0x65, 0x70, 0x6f, 0x65, 0x6e, 0x64 };

uint8_t getBitByIndex(uint8_t byte, uint8_t index)
{
	return (byte >> index) & 1;
}

void setBitByIndex(uint8_t *pByte, uint8_t index)
{
	*pByte |= (1 << index);
}

void cbMakeEven(pixel_t *pixel)
{
	pixel->red   -= pixel->red   % 2;
	pixel->green -= pixel->green % 2;
	pixel->blue  -= pixel->blue  % 2;
}

void checkEven(pixel_t *pixel)
{
	if (pixel->red % 2 != 0 || pixel->green % 2 != 0 || pixel->blue % 2 != 0)
	{
		printf("Found illegal pixel\n");
	}
}

void forEachPixel(image_t *image, pixelCallback_t callback)
{
	for (int i = 0; i < image->width * image->height; i++)
	{
		callback(&image->pixels[i]);
	}
}

bool arrayEndsWith(uint8_t *needle, uint8_t *haystack, int haystackSize, int needleLength)
{
	bool matching = true;
	while (matching)
	{
		matching = needle[--needleLength] == haystack[--haystackSize];
		//printf("needle [%c] == [%c] haystack | Equal: %d needlelength: %d\n", needle[needleLength], haystack[haystackSize], haystack[haystackSize] == needle[needleLength], needleLength);
	}

	return needleLength < 0;
}

bool writePNG(const char *filePath, image_t *image) {

	FILE *fp = fopen(filePath, "wb");
	if (!fp) abort();

	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) abort();

	png_infop info = png_create_info_struct(png);
	if (!info) abort();

	if (setjmp(png_jmpbuf(png))) abort();

	png_init_io(png, fp);

	png_set_IHDR(
		png,
		info,
		image->width, image->height,
		8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT
	);

	//png_set_expand(png);

	png_bytepp rowPointers = (png_bytepp)png_malloc(png, image->height * sizeof(png_bytep));

	if (!rowPointers)
	{
		if (PRINT_ERRORS)
			printf("Could not allocate row pointers for result png. Aborting!\n");
		return false;
	}

	//Allocate space for rows and transfer bitmap data
	for (int y = 0; y < image->height; y++)
	{
		rowPointers[y] = (png_bytep)png_malloc(png, sizeof(uint8_t) * image->width * PNG_PIXELSIZE); // 4 being the pixel_size (rgba, 1 byte each)

		for (int x = 0; x < image->width; x++)
		{
			pixel_t *currentPixel = image->pixels + image->width * y + x;

			rowPointers[y][x * PNG_PIXELSIZE + 0] = currentPixel->red;
			rowPointers[y][x * PNG_PIXELSIZE + 1] = currentPixel->green;
			rowPointers[y][x * PNG_PIXELSIZE + 2] = currentPixel->blue;
			rowPointers[y][x * PNG_PIXELSIZE + 3] = currentPixel->alpha;

		}
	}

	//Write bitmap data to the png file
	png_write_info(png, info);
	png_write_image(png, rowPointers);
	png_write_end(png, NULL);


	//Free stuff
	for (int y = 0; y < image->height; y++)
	{
		png_free(png, rowPointers[y]);
	}
	png_free(png, rowPointers);
	png_destroy_write_struct(&png, &info);
	fclose(fp);

	//Everything worked
	return true;
}


bool readPNG(const char *filePath, image_t *outImage)
{

	FILE *fp = fopen(filePath, "rb");

	if (!fp)
	{
		if (PRINT_ERRORS)
			printf("Could not open file in readPNG()!\n");
		return false;
	}

	//Check if its a png file
	void *firstBytes = malloc(PNGSIGN_LENGTH);
	fread(firstBytes, 1, PNGSIGN_LENGTH, fp);
	if (png_sig_cmp((png_bytep)firstBytes, 0, PNGSIGN_LENGTH))
	{
		if (PRINT_ERRORS)
			printf("Passed file is not a png. Aborting!\n");
		fclose(fp);
		free(firstBytes);
		return false;
	}

	png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png)
	{
		if (PRINT_ERRORS)
			printf("Couldnt create read struct!\n");
		return false;
	}

	png_infop info = png_create_info_struct(png);

	if (!info)
	{
		if (PRINT_ERRORS)
			printf("Couldnt create info struct!\n");
		return false;
	}

	if (setjmp(png_jmpbuf(png)))
	{
		return false;
	}

	//rgb index instead of colour index, where is the diff??? http://jeromebelleman.gitlab.io/posts/devops/libpng/
	//png_set_palette_to_rgb(png);

	//Init pnglib
	png_init_io(png, fp);
	png_set_sig_bytes(png, PNGSIGN_LENGTH);
	png_read_info(png, info);

	//Read out some metadata
	outImage->width = png_get_image_width(png, info);
	outImage->height = png_get_image_height(png, info);
	png_byte color_type = png_get_color_type(png, info);
	png_byte bit_depth = png_get_bit_depth(png, info);


	if (bit_depth == 16)
	{
		if (PRINT_DEBUG)
			printf("Bit depth was 16. Setting strip to 16!\n");

		png_set_strip_16(png);
	}

	if (color_type == PNG_COLOR_TYPE_PALETTE)
	{
		if (PRINT_DEBUG)
			printf("Color type was palette. Setting to RGB!\n");

		png_set_palette_to_rgb(png);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	{
		if (PRINT_DEBUG)
			printf("Color type was gray and bit depth < 8. Expanding to 8!\n");

		png_set_expand_gray_1_2_4_to_8(png);
	}

	if (png_get_valid(png, info, PNG_INFO_tRNS))
	{
		if (PRINT_DEBUG)
			printf("No idea but tRNS smth!\n");

		png_set_tRNS_to_alpha(png);
	}

	if (color_type == PNG_COLOR_TYPE_RGB ||
		color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_PALETTE)
	{
		if (PRINT_DEBUG)
			printf("Color type was either rgb || gray || palette. Setting filler to 0xFF (AFTER)\n");

		png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	{
		if (PRINT_DEBUG)
			printf("Color type was either gray || gray_alpha. Setting gray to rgb!\n");

		png_set_gray_to_rgb(png);
	}

	png_read_update_info(png, info);

	//png_bytepp rowPointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
	png_bytepp rowPointers = (png_bytepp)png_malloc(png, outImage->height * sizeof(png_bytep));

	for (int y = 0; y < outImage->height; y++)
	{
		//rowPointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
		rowPointers[y] = (png_bytep)png_malloc(png, png_get_rowbytes(png, info));
	}

	//Actually read the image pixels
	png_read_image(png, rowPointers);
	png_read_end(png, info);

	//Allocate some space to store pixels in our own datatype
	outImage->pixels = (pixel_t *)calloc(outImage->width * outImage->height, sizeof(pixel_t));

	if (!outImage->pixels)
	{
		if (PRINT_ERRORS)
			printf("Could not allocate pixels for result image!\n");

		return false;
	}

	//Copy all pixels to the outImage
	for (int y = 0; y < outImage->height; y++)
	{
		png_bytep row = rowPointers[y];
		for (int x = 0; x < outImage->width; x++)
		{
			png_bytep pixel = &(row[x * PNG_PIXELSIZE]);

			outImage->pixels[y * outImage->width + x].red = pixel[0];
			outImage->pixels[y * outImage->width + x].green = pixel[1];
			outImage->pixels[y * outImage->width + x].blue = pixel[2];
			outImage->pixels[y * outImage->width + x].alpha = pixel[3];

		}
	}

	//Free stuff (question for rick: how is y local to the for loop in final .exe?)
	for (int y = 0; y < outImage->height; y++)
	{
		png_free(png, rowPointers[y]);
	}
	png_free(png, rowPointers);

	png_destroy_read_struct(&png, &info, NULL);
	fclose(fp);

	//Wrote image data to outImage!
	return true;
}


long readPayload(const char *fileName, uint8_t **destination)
{
	FILE *fp = fopen(fileName, "rb");
	if (!fp)
	{
		if (PRINT_ERRORS)
			printf("Could not read payload file (%s). Aborting!\n", fileName);

		return 0;
	}

	fseek(fp, 0L, SEEK_END);
	long fileSize = ftell(fp);
	if (fileSize < 0)
	{
		if (PRINT_ERRORS)
			printf("ftell failed. Aborting!\n");

		return 0;
	}
	*destination = (uint8_t *)malloc(sizeof(**destination) * (fileSize + 1));
	if (!*destination)
	{
		if (PRINT_ERRORS)
			printf("Could not allocate space for payload. Aborting!\n");

		return 0;
	}
	fseek(fp, 0L, SEEK_SET);

	uint32_t length = fread(*destination, sizeof(uint8_t), fileSize, fp);
	(*destination)[length++] = 0;
	fclose(fp);

	return fileSize;
}

injectInfo_t writeHiddenBytes(image_t *image, uint8_t *payload, long payloadLength, injectInfo_t *indices)
{
	int currentPayloadByte = 0;
	int currentPayloadBit = 7;
	int pixelIndex = 0;
	int channel = 0;
	bool abortLoops = false;

	//Keep track of last pixel we write to and what channel it was for further writing
	injectInfo_t info = { 0 };

	//Use info from previous write if there is any
	if (indices)
	{
		pixelIndex = indices->pixelIndex;
		channel = indices->channel;
	}

	for (pixelIndex; (pixelIndex < image->width * image->height) && !abortLoops; pixelIndex++)
	{
		pixel_t *pixel = &image->pixels[pixelIndex];

		for (channel; (channel < 3) && !abortLoops; channel++)
		{
			if (currentPayloadByte < payloadLength)
			{
				if (PRINT_DEBUG)
					printf("[Debug] Char: %c (Bit %d = %d ) | At Pixel: %d in channel: %d \n", payload[currentPayloadByte], currentPayloadBit, getBitByIndex(payload[currentPayloadByte], currentPayloadBit), pixelIndex, channel);
				
				if (getBitByIndex(payload[currentPayloadByte], currentPayloadBit))
				{
					*(((uint8_t *)pixel) + channel) += 1;

					if (PRINT_DEBUG)
						printf("\tIncremented\n");
				}
				if (--currentPayloadBit < 0)
				{
					currentPayloadBit = 7;
					currentPayloadByte++;
				}
			}
			else
			{
				//We're done writing and save pixelIndex and channel we last wrote to
				if (PRINT_DEBUG)
					printf("Done writing payload bytes.\n");

				abortLoops = true;
				info.channel = channel;
				info.pixelIndex = pixelIndex;
			}
		}

		channel = 0;
	}


	return info;
}

bool injectPayload(image_t *image, uint8_t *payload, long payloadLength)
{
	//First check if the image has enough space for the payload
	long maxPayloadLength = (long)((image->width * image->height * 3) / 8);
	bool canHostPayload = payloadLength < (maxPayloadLength - STEG_SIGN_START_LENGTH - STEG_SIGN_END_LENGTH);

	if (PRINT_INFO)
		printf("The provided image can hold %d hidden bytes. The payload has %d bytes...%s\n", maxPayloadLength, payloadLength, canHostPayload ? "OK" : "Aborting");

	if (!canHostPayload)
	{
		return false;
	}

	//Write the _STEG_PAYLOAD_START signature
	injectInfo_t indices = writeHiddenBytes(image, _STEG_PAYLOAD_START, STEG_SIGN_START_LENGTH, NULL);
	
	printf("\nWriting payload now... Starting from pixel %d, channel %d\n\n", indices.pixelIndex, indices.channel);
	//Write the payload
	indices = writeHiddenBytes(image, payload, payloadLength, &indices);

	printf("\nWriting delimiter now... Starting from pixel %d, channel %d\n\n", indices.pixelIndex, indices.channel);
	//Write the _STEG_PAYLOAD_END signature
	writeHiddenBytes(image, _STEG_PAYLOAD_END, STEG_SIGN_END_LENGTH, &indices);

	//Success
	return true;
}

bool containsPayload(image_t *image)
{
	uint8_t *tmp = (uint8_t *)calloc(STEG_SIGN_START_LENGTH + 1, sizeof(uint8_t));
	if (!tmp)
	{
		if (PRINT_ERRORS)
			printf("Couldnt allocate temporary buffer. Aborting!\n");

		return false;
	}

	int currentPayloadBit = 7;
	uint8_t resultByte = 0;
	uint32_t bytesRead = 0;

	// Note: + 2 because of integer div
	int maxPixelIndex = (STEG_SIGN_START_LENGTH * 8) / 3 + 2; 

	//We only check as many pixels are needed to store the start signature 
	for (int pixelIndex = 0; pixelIndex < maxPixelIndex; pixelIndex++)
	{
		pixel_t *pixel = &image->pixels[pixelIndex];
		for (int channel = 0; channel < 3; channel++)
		{
			if (*(((uint8_t *)pixel) + channel) % 2 != 0)
			{
				setBitByIndex(&resultByte, currentPayloadBit);
			}

			if (--currentPayloadBit < 0) {
				currentPayloadBit = 7;
				tmp[bytesRead++] = resultByte;
				//printf("Read one entire byte: '%d' Adding '%c' to result! %s\n", resultByte, resultByte, tmp);
				resultByte = 0;

				if (memcmp(_STEG_PAYLOAD_START, tmp, STEG_SIGN_START_LENGTH) == 0)
				{
					free(tmp);
					return true;
				}
			}
		}
	}

	free(tmp);
	return false;
}

int extractPayload(image_t *image, uint8_t **destination)
{
	//This is the highest amount of bytes that could possibly be stored in the png
	uint32_t maxPayloadLength = (uint32_t)((image->width * image->height * 3) / 8);

	//Allocate enough space to accomodate that many bytes
	uint8_t *tmpBuffer = (uint8_t *)calloc(maxPayloadLength, sizeof(uint8_t));
	

	if (!tmpBuffer)
	{
		if (PRINT_ERRORS)
			printf("Could not allocate space for extracted payload. Aborting!\n");

		return 0;
	}

	int currentPayloadBit = 7;
	uint8_t resultByte = 0;
	uint32_t bytesRead = 0;
	bool abortLoops = false;

	for (int pixelIndex = 0; (pixelIndex < image->width * image->height) && !abortLoops; pixelIndex++)
	{
		pixel_t *pixel = &image->pixels[pixelIndex];
		for (int channel = 0; (channel < 3) && !abortLoops; channel++)
		{
			//printf("[Debug] At Pixel: %d in channel: %d | %dth bit -> %d | Extracted: %s\n", pixelIndex, channel, currentPayloadBit, (*(((uint8_t *)pixel) + channel) % 2 == 0) ? 0 : 1, *destination);

			//If the pixels current channel is odd, its our encoded bit data
			if (*(((uint8_t *)pixel) + channel) % 2 != 0)
			{
				setBitByIndex(&resultByte, currentPayloadBit);
			}

			//Did we read 8 bits already? (Going from 7 -> 0)
			if (--currentPayloadBit < 0) {

				//We read 8 bits and can save it as a byte for the result
				currentPayloadBit = 7;
				tmpBuffer[bytesRead++] = resultByte;
				//printf("Read one entire byte: '%d' Adding '%c' to result!\n", resultByte, resultByte);
				resultByte = 0;

				//Check if we read our 'EOF' signature and if so abort
				if (arrayEndsWith(_STEG_PAYLOAD_END, tmpBuffer, bytesRead, STEG_SIGN_END_LENGTH))
				{
					abortLoops = true;
					
					//We dont want to store our EOF signature in the dump
					bytesRead -= (STEG_SIGN_START_LENGTH + STEG_SIGN_END_LENGTH);

					//Finally save the payload to the destination buffer
					*destination = (uint8_t *)calloc(bytesRead, sizeof(uint8_t));
					memcpy(*destination, tmpBuffer + STEG_SIGN_START_LENGTH, bytesRead);
					free(tmpBuffer);
					break;
				}
			}
		}
	}

	return bytesRead;
}