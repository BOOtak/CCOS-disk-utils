#ifndef COMMON_H
#define COMMON_H

#include <ccos_image.h>

#include <stdint.h>
#include <stdio.h>

#if defined(WIN32)
#define MKDIR(filename, mode) mkdir(filename)
#else
#define MKDIR(filename, mode) mkdir(filename, mode)
#endif

#define UNUSED __attribute__((unused))

#define VERSION_MAX_SIZE 12  // "255.255.255"

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
 * @param[in]  disk             Compass disk image.
 * @param[in]  in_place         If true, override original target image. Otherwise, save new image under
 * {target_image}.out name.
 *
 * @return     0 on success, -1 otherwise.
 */
int save_image(const char* source_filename, ccos_disk_t* disk, int in_place);

const char* get_basename(const char* path);

#endif  // COMMON_H
