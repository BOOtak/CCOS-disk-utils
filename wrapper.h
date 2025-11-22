#ifndef WRAPPER_H
#define WRAPPER_H

#include <stdint.h>

/**
 * @brief      Dumps a directory recursively from CCOS disk image.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir(ccfs_handle ctx, const char* path, ccos_inode_t* dir, uint8_t* data);

/**
 * @brief      Dumps all files and directories from CCOS disk image.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param[in]  path       The path to CCOS image.
 * @param[in]  data       CCOS image data.
 * @param[in]  data_size  Image data size.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image(ccfs_handle ctx, const char* path, uint8_t* data, size_t data_size);

/**
 * @brief      Dumps file to directory from CCOS disk image.
 *
 * @param[in]  ctx              Filesystem context handle.
 * @param[in]  path_to_dir      The path to destination directory.
 * @param[in]  file             The file to dump.
 * @param[in]  image_data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_file(ccfs_handle ctx, const char* path_to_dir, ccos_inode_t* file, uint8_t* image_data);

/**
 * @brief      Dumps a directory recursively from CCOS disk image to a custom folder.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param[in]  path       The path to CCOS image.
 * @param[in]  dir        The directory.
 * @param[in]  data       CCOS image data.
 * @param[in]  destpath   The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir_to(ccfs_handle ctx, const char* path, ccos_inode_t* dir, uint8_t* data, const char* destpath);

/**
 * @brief      Dumps all files and directories from CCOS disk image to a custom folder.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param[in]  path       The path to CCOS image.
 * @param[in]  data       CCOS image data.
 * @param[in]  data_size  Image data size.
 * @param[in]  destpath   The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image_to(ccfs_handle ctx, const char* path, uint8_t* data, size_t data_size, const char* destpath);

/**
 * @brief      Prints a CCOS image contents.
 *
 * @param[in]  ctx           Filesystem context handle.
 * @param[in]  path          The path to CCOS image.
 * @param[in]  data          CCOS image data.
 * @param[in]  data_size     Image data size.
 * @param[in]  short_format  Use shorter, 80-column compatible output format.
 *
 * @return     0 on success, -1 otherwise.
 */
int print_image_info(ccfs_handle ctx, const char* path, uint8_t* data, size_t data_size, int short_format);

/**
 * @brief      Replace file in the CCOS image.
 *
 * @param[in]  ctx          Filesystem context handle.
 * @param[in]  path         Path to the CCOS image.
 * @param[in]  filename     Path to the file to replace in the CCOS image.
 * @param[in]  target_name  If given, then the file with this name will be replaced in the CCOS image. Otherwise,
 * basename of the filename will be used.
 * @param[in]  data         CCOS image data.
 * @param[in]  data_size    CCOS image data size.
 * @param[in]  in_place     Flag indicating whether to save replaced file under a new name (path.new), or overwrite the
 * original file.
 *
 * @return     0 on success, -1 otherwise.
 */
int replace_file(ccfs_handle ctx, const char* path, const char* filename, const char* target_name, uint8_t* data, size_t data_size,
                 int in_place);

/**
 * @brief      Copy file from one image into another.
 *
 * @param[in]  ctx           Filesystem context handle.
 * @param[in]  target_image  Path to the image to copy file to.
 * @param[in]  filename      The name of file to copy.
 * @param[in]  source_data   CCOS source image data.
 * @param[in]  source_size   CCOS source image data size.
 * @param[in]  in_place      If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int copy_file(ccfs_handle ctx, const char* target_image, const char* filename, uint8_t* source_data, size_t source_size, int in_place);

/**
 * @brief      Delete file in the image.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param[in]  path        Path to the image to delete file in.
 * @param[in]  filename    The name of file to delete.
 * @param[in]  in_place    If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int delete_file(ccfs_handle ctx, const char* path, const char* filename, int in_place);

/**
 * @brief      Add file to the image.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param[in]  image_path  Path to the image to add file.
 * @param[in]  file_path   The path to file to add.
 * @param[in]  file_name   The name of file to add.
 * @param[in]  data        CCOS image data.
 * @param[in]  data_size   CCOS image data size.
 * @param[in]  in_place    If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file(ccfs_handle ctx, const char* image_path, const char* file_path, const char* file_name,
             uint8_t* data, size_t data_size, int in_place);

/**
 * @brief      Create directory in the image.
 *
 * @param[in]  ctx            Filesystem context handle.
 * @param[in]  path           Path to the image to create dir.
 * @param[in]  directory_name The name of directory to create.
 * @param[in]  file_contents  CCOS image data.
 * @param[in]  file_size      CCOS image data size.
 * @param[in]  in_place       If true, override original target image. Otherwise, save new image under
 * {target_image}.out name.
 *
 * @return     0 on success, -1 otherwise.
 */
int create_directory(ccfs_handle ctx, char* path, char* directory_name, uint8_t* file_contents, size_t file_size, int in_place);

/**
 * @brief      Rename file in the image.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param      path        Path to the image.
 * @param      file_name   The current name of the file.
 * @param      new_name    The new name of the file.
 * @param      image_data  CCOS image data.
 * @param[in]  image_size  CCOS image data size.
 * @param[in]  in_place    If true, overwrite original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int rename_file(ccfs_handle ctx, char* path, char* file_name, char* new_name, uint8_t* image_data, size_t image_size, int in_place);

/**
 * @brief      Create new blank CCOS image file.
 *
 * @param[in]  ctx   Filesystem context handle.
 * @param[in]  path  Path where to create the new image file.
 * @param[in]  size  Size of the image in bytes. Must be a multiple of the sector size.
 *
 * @return     0 on success, -1 otherwise.
 */
int create_blank_image(ccfs_handle ctx, char* path, size_t size);

#endif  // WRAPPER_H
