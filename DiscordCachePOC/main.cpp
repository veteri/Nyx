#include "steganography.h"

#define PAYLOAD_IMAGE_PATH_MAXLENGTH 0x200

enum OPERATION { OP_NONE, OP_INJECT, OP_EXTRACT, OP_CHECK };

/*

-inject -i rarechristmas.png -p hi.txt
-extract -i rarechristmas_payload.png -o hi2.txt
-check rarechristmas_payload.png

-inject -i rarechristmas.png -p DiscordPayloadDLL.dll
-extract -i rarechristmas_payload.png -o test.dll

*/

int main(int argc, char **argv)
{

	OPERATION op = OP_NONE;
	if (strcmp(argv[1], "-inject") == 0)
	{
		op = OP_INJECT;
	}
	else if (strcmp(argv[1], "-extract") == 0)
	{
		op = OP_EXTRACT;
	}
	else if (strcmp(argv[1], "-check") == 0)
	{
		op = OP_CHECK;
	}

	if (op == OP_NONE)
	{
		printf("Usage:\n1. Inject a payload:\n\t -inject -i {path to png image to inject into} -p {path to payload file}\n\n2. Extract a payload:\n\t -extract -i {path to png image to extract from} -o {path to output file}");
		return 1;
	}

	switch (op)
	{
	case OP_CHECK:
	{
		image_t image;
		char *inputImagePath = argv[2];
		bool pngReadSuccess = readPNG(inputImagePath, &image);
		if (!pngReadSuccess)
		{
			printf("Could not read specified png file. Aborting!\n");
			return 1;
		}

		bool hasPayload = containsPayload(&image);
		printf("The image contains %s payload.\n", hasPayload ? "a" : "no");

		break;
	}
	case OP_INJECT: 
	{
		image_t image;
		char *inputImagePath = argv[3];
		char *payloadPath = argv[5];
		bool pngReadSuccess = readPNG(inputImagePath, &image);

		if (!pngReadSuccess)
		{
			printf("Could not read specified png file. Aborting!\n");
			return 1;
		}

		//Read payload data to buffer
		uint8_t *payload = nullptr;
		long payloadLength = readPayload(payloadPath, &payload);
		printf("Payload has size of %d bytes\n\n", payloadLength);


		//Make all pixel values even (set our memory to all 0 bytes basically)
		forEachPixel(&image, cbMakeEven);
		
		/*
		//To be considered? only make those even that we need for storage (probably better for hiding stuff)
		long pixelsNeeded = ((STEG_SIGN_START_LENGTH + payloadLength +  STEG_SIGN_END_LENGTH) * 8) / 3 + 2;
		for (int i = 0; i < pixelsNeeded; i++)
		{
			cbMakeEven(&(image.pixels[i]));
		}
		printf("Made %d pixels channels even.\n", pixelsNeeded);
		*/

		//Inject payload into image
		bool injectSuccess = injectPayload(&image, payload, payloadLength);

		if (!injectSuccess)
		{
			printf("Failed to inject payload. Aborting!\n");
			return 1;
		}

		printf("Successfully injected payload!\n");

		//Save processed image
		char *processedImagePath = (char *)calloc(PAYLOAD_IMAGE_PATH_MAXLENGTH, sizeof(char));
		if (!processedImagePath)
		{
			printf("Couldnt allocate space for the name of result image. Aborting!\n");
			return 1;
		}

		//Construct name for the result image: {oldName}_payload.png
		memcpy(processedImagePath, inputImagePath, strlen(inputImagePath) - 4);
		strcat(processedImagePath, "_payload.png");

		bool pngWriteSuccess = writePNG(processedImagePath, &image);
		if (!pngWriteSuccess)
		{
			printf("Could not write result png file. Aborting!\n");
			return 1;
		}

		//Free stuff
		free(payload);
		free(processedImagePath);
		free(image.pixels);
		break;
	}
	case OP_EXTRACT: 
	{
		image_t image;
		char *inputImagePath = argv[3];
		char *payloadResultPath = argv[5];
		bool pngReadSuccess = readPNG(inputImagePath, &image);
		if (!pngReadSuccess)
		{
			printf("Could not read specified png file. Aborting!\n");
			return 1;
		}
		uint8_t *extractedPayload = nullptr;
		int bytesRead = extractPayload(&image, &extractedPayload);
		printf("Extracted payload is: %s\n", extractedPayload);
		FILE *fp = fopen(payloadResultPath, "wb+");
		if (!fp)
		{
			printf("Couldnt open '%s' to save the payload. Aborting!\n", payloadResultPath);
			return 1;
		}
		printf("Dumping extracted data to %s\n", payloadResultPath);
		fwrite(extractedPayload, sizeof(uint8_t), bytesRead, fp);

		//Free stuff
		fclose(fp);
		//This crashes cause we move the pointer in extractPayload
		free(extractedPayload);
		free(image.pixels);
		break;
	}
	}

	return 0;
}

