#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdint.h>

/**
 * @brief      Dumps a directory recursively from CCOS disk image.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir(const char* path, ccos_inode_t* dir, uint8_t* data);

/**
 * @brief      Dumps all files and directories from CCOS disk image.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image(const char* path, uint8_t* data, size_t data_size);

/**
 * @brief      Dumps file to directory from CCOS disk image.
 *
 * @param[in]  path_to_dir      The path to CCOS image.
 * @param[in]  file             The file.
 * @param[in]  image_data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_file(const char* path_to_dir, ccos_inode_t* file, uint8_t* image_data);

/**
 * @brief      Dumps a directory recursively from CCOS disk image to a custom folder.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 * @param[in]  destpath   The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir_to(const char* path, ccos_inode_t* dir, uint8_t* data, const char* destpath);

/**
 * @brief      Dumps all files and directories from CCOS disk image to a custom folder.
 *
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 * @param[in]  destpath   The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image_to(const char* path, uint8_t* data, size_t data_size, const char* destpath);

/**
 * @brief      Prints a CCOS image contents.
 *
 * @param[in]  path          The path to CCOS image.
 * @param[in]  superblock    The superblock inode.
 * @param[in]  data          CCOS image data.
 * @param[in]  short_format  Use shorter, 80-column compatible output format.
 *
 * @return     0 on success, -1 otherwise.
 */
int print_image_info(const char* path, uint8_t* data, size_t data_size, int short_format);

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
int replace_file(const char* path, const char* filename, const char* target_name, uint8_t* data, size_t data_size,
                 int in_place);

/**
 * @brief      Copy file from one image into another.
 *
 * @param[in]  target_image  Path to the image to copy file to.
 * @param[in]  filename      The name of file to copy.
 * @param[in]  superblock    The superblock in the source image.
 * @param[in]  source_data   CCOS source image data.
 * @param[in]  source_size   CCOS source image data size.
 * @param[in]  in_place      If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int copy_file(const char* target_image, const char* filename, uint8_t* source_data, size_t source_size, int in_place);

/**
 * @brief      Delete file in the image.
 *
 * @param[in]  path        Path to the image to delete file in.
 * @param[in]  filename    The name of file to delete.
 * @param[in]  in_place    If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int delete_file(const char* path, const char* filename, int in_place);

/**
 * @brief      Add file to the image.
 *
 * @param[in]  image_path  Path to the image to add file.
 * @param[in]  file_path   The path to file to add.
 * @param[in]  file_name   The name of file to delete.
 * @param[in]  data        CCOS image data.
 * @param[in]  data_size   CCOS image data size.
 * @param[in]  in_place    If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file(const char* image_path, const char* file_path, const char* file_name, uint8_t* data, size_t data_size,
             int in_place);

/**
 * @brief      Create directory in the image.
 *
 * @param[in]  path           Path to the image to create dir.
 * @param[in]  directory_name The name of file to delete.
 * @param[in]  file_contents  CCOS image data.
 * @param[in]  file_size      CCOS image data size.
 * @param[in]  in_place       If true, override original target image. Otherwise, save new image under
 * {target_image}.out name.
 *
 * @return     0 on success, -1 otherwise.
 */
int create_directory(char* path, char* directory_name, uint8_t* file_contents, size_t file_size, int in_place);

int rename_file(char* path, char* file_name, char* new_name, uint8_t* image_data, size_t image_size, int in_place);

int create_blank_image(char* path);

#endif  // WRAPPER_H
