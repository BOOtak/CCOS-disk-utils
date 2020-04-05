#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>

#define VERSION_MAX_SIZE 12  // "255.255.255"

#define MIN(A, B) A < B ? A : B

#define TRACE(format, ...)                                                        \
  if (trace != NULL) {                                                            \
    trace(stderr, "%s:%d:\t" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
  }

int (*trace)(FILE* stream, const char* format, ...);

int trace_silent(FILE* stream, const char* format, ...);

/**
 * @brief      Initialize trace function
 *
 * @param[in]  verbose  Verbosity level (0 for no output in TRACE call, other values: maximum verbosity)
 */
void trace_init(int verbose);

int read_file(const char* path, uint8_t** file_data, size_t* file_size);

#endif  // COMMON_H
