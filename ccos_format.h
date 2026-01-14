#ifndef CCOS_DISK_TOOL_CCOS_FORMAT_H
#define CCOS_DISK_TOOL_CCOS_FORMAT_H

#include "ccos_context.h"

#include <stdbool.h>
#include <stdlib.h>

/**
 * @brief      Creates a new empty CCOS disk image.
 *
 * @param[in]  sector_size   The sector size in bytes. Supported values: 256 or 512.
 * @param[in]  bytes         Total size of the image in bytes. Must be a multiple of sector_size.
 * @param[out] output        Disk image output.
 *
 * @return     0 on success, or an error code.
 */
int ccos_new_disk_image(uint16_t sector_size, size_t bytes, ccos_disk_t* output);

#endif  // CCOS_DISK_TOOL_CCOS_FORMAT_H
