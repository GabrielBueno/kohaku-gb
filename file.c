#include "file.h"

#include <stdio.h>
#include <assert.h>

#include "log.h"

enum file_result file_read(struct file *out, const char *path) {
    assert(out != NULL);
	assert(path != NULL);

	FILE *file = fopen(path, "rb");

	if (file == NULL) {
		ERROR("couldn't open file %s.", path);
		return FILE_ERROR;
	}

	out->data   = NULL;
	out->length = 0;

	size_t  fsize      = 0;
	size_t  bytes_read = 0;
	uint8_t *data      = NULL;

	fseek(file, 0, SEEK_END);
	fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	data = malloc(fsize);

	if (data == NULL) {
		ERROR("couldn't allocate memory.");
		goto error;
	}

	bytes_read = fread(data, sizeof(uint8_t), fsize, file);

	if (bytes_read != fsize) {
		ERROR("%zu bytes was expected, but only %zu were read.", fsize, bytes_read);
		goto error;
	}

	out->data   = data;
	out->length = fsize;

	fclose(file);
	return FILE_OK;

error:
	free(data);
	fclose(file);

	return FILE_ERROR;
}