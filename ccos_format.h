#ifndef CCOS_DISK_TOOL_CCOS_FORMAT_H
#define CCOS_DISK_TOOL_CCOS_FORMAT_H

#include "ccos_disk.h"

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    CCOS_DISK_FORMAT_COMPASS,
    CCOS_DISK_FORMAT_BUBMEM,
    CCOS_DISK_FORMAT_GRIDCASE,
} disk_format_t;

/**
 * @brief      Creates a new empty CCOS disk image.
 *
 * @param[in]  format        The disk format to use (e.g., DISK_FORMAT_COMPASS).
 * @param[in]  disk_size     Total size of the image in bytes, should be a multiple of 512.
 * @param[out] output        Pointer to the resulting disk image structure.
 *
 * @return     0 on success, or an error code.
 */
int ccos_new_disk_image(disk_format_t format, size_t disk_size, ccos_disk_t* output);

#endif  // CCOS_DISK_TOOL_CCOS_FORMAT_H
