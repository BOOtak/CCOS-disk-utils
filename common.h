#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdio.h>

#if defined(WIN32)
#define SIZE_T "%I64d"
#define MKDIR(filename, mode) mkdir(filename)
#else
#define SIZE_T "%ld"
#define MKDIR(filename, mode) mkdir(filename, mode)
#endif

#define UNUSED __attribute__((unused))

#define VERSION_MAX_SIZE 12  // "255.255.255"

#define MIN(A, B) A < B ? A : B

#define TRACE(format, ...)                                                          \
  do {                                                                              \
    if (trace != NULL) {                                                            \
      trace(stderr, "%s:%d:\t" format "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    }                                                                               \
  } while (0)

extern int (*trace)(FILE* stream, const char* format, ...);

int trace_silent(FILE* stream, const char* format, ...);

/**
 * @brief      Initialize trace function
 *
 * @param[in]  verbose  Verbosity level (0 for no output in TRACE call, other values: maximum verbosity)
 */
void trace_init(int verbose);

/**
 * @brief      Read file contents from path into memory buffer
 *
 * @param[in]  path        File path to read.
 * @param      file_data   The file data.
 * @param      file_size   The file size.
 *
 * @return     0 on success, -1 otherwise.
 */
int read_file(const char* path, uint8_t** file_data, size_t* file_size);

/**
 * @brief      Save memory buffer content to file
 *
 * @param[in]  source_filename  File path to save.
 * @param[in]  data             The file data.
 * @param[in]  data_size        The file size.
 * @param[in]  in_place         If true, override original target image. Otherwise, save new image under
 * {target_image}.out name.
 *
 * @return     0 on success, -1 otherwise.
 */
int save_image(const char* source_filename, uint8_t* data, size_t data_size, int in_place);

const char* get_basename(const char* path);

#endif  // COMMON_H
