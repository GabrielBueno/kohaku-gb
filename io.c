#include "io.h"

#include <stdio.h>

void ReadFile(const char* path, struct Buffer* buffer)
{
	if (buffer == NULL)
	{
		return NULL;
	}

	if (path == NULL)
	{
		fprintf(stderr, "[ReadFile()] received a NULL pointer\n");
		return NULL;
	}

	FILE* file = fopen(path, "rb");

	if (file == NULL)
	{
		fprintf(stderr, "couldn't open file %s\n", path);
		return NULL;
	}

	fseek(file, 0, SEEK_END);

	size_t   fsize    = ftell(file);
	uint8_t* contents = malloc(fsize * sizeof(uint8_t));

	fseek(file, 0, SEEK_SET);
	fread(contents, sizeof(uint8_t), fsize, file);
	fclose(file);

	buffer->data = contents;
	buffer->size = fsize;
}