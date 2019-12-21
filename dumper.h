#ifndef DUMPER_H
#define DUMPER_H

#include <stdint.h>

/**
 * @brief      Dumps a directory recirsively from CCOS disk image.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir_inode  The directory inode.
 * @param[in]  data       CCOS image data.
 * @param[in]  verbose    Verbose log output.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir(const char* path, const uint16_t dir_inode, const uint8_t* data, int verbose);

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

/**
 * @brief      Replace file in the CCOS image.
 *
 * @param[in]  path         Path to the CCOS image.
 * @param[in]  filename     Path to the file to replace in the CCOS image.
 * @param[in]  target_name  If given, then the file with this name will be replaced in the CCOS image. Otherwise,
 * basename of the filename will be used.
 * @param[in]  superblock   The superblock inode.
 * @param      data         CCOS image data.
 * @param[in]  data_size    CCOS image data size.
 * @param[in]  in_place     Flag indicating whether to save replaced file under a new name (path.new), or overwrite the
 * original file.
 *
 * @return     0 on success, -1 otherwise.
 */
int replace_file(const char* path, const char* filename, const char* target_name, const uint16_t superblock,
                 uint8_t* data, size_t data_size, int in_place);

#endif  // DUMPER_H
