#ifndef DUMPER_H
#define DUMPER_H

#include <stdint.h>

/**
 * @brief      Dumps a directory recirsively from CCOS disk image.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir_inode  The directory inode.
 * @param[in]  data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir(const char* path, const uint16_t dir_inode, const uint8_t* data);

/**
 * @brief      Prints a CCOS image contents.
 *
 * @param[in]  path        The path to CCOS image.
 * @param[in]  superblock  The superblock inode.
 * @param[in]  data        CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int print_image_info(const char* path, const uint16_t superblock, const uint8_t* data);

#endif // DUMPER_H
