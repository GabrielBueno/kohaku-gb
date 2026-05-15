#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stdlib.h>

enum file_result {
    FILE_OK    = 0,
    FILE_ERROR = 1,
};

struct file {
    uint8_t *data;
    size_t length;
};

enum file_result file_read(struct file *file, const char *path);

#endif