#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#define ERROR(fmt, ...) fprintf(stderr, "E: [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt,  ...) fprintf(stderr, "W: [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt,  ...) fprintf(stderr, "I: [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG(fmt, ...) fprintf(stderr, "D: [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define FATAL(fmt, ...) { fprintf(stderr, "D: [%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); exit(1); }

#endif