#ifndef WRAPPER_H
#define WRAPPER_H

#include "ccos_disk.h"
#include "ccos_structure.h"

#include <stdint.h>

/**
 * @brief      Dumps a directory recursively from CCOS disk image.
 *
 * @param[in]  disk  Compass disk image.
 * @param[in]  path  The path to CCOS image.
 * @param[in]  dir   The directory.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir(ccos_disk_t* disk, const char* path, ccos_inode_t* dir);

/**
 * @brief      Dumps all files and directories from CCOS disk image.
 *
 * @param[in]  disk  Compass disk image.
 * @param[in]  path  The path to CCOS image.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image(ccos_disk_t* disk, const char* path);

/**
 * @brief      Dumps file to directory from CCOS disk image.
 *
 * @param[in]  disk             Compass disk image.
 * @param[in]  path_to_dir      The path to destination directory.
 * @param[in]  file             The file to dump.
 * @param[in]  image_data       CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_file(ccos_disk_t* disk, const char* path_to_dir, ccos_inode_t* file, uint8_t* image_data);

/**
 * @brief      Dumps a directory recursively from CCOS disk image to a custom folder.
 *
 * @param[in]  disk      Compass disk image.
 * @param[in]  path      The path to CCOS image.
 * @param[in]  dir       The directory.
 * @param[in]  destpath  The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_dir_to(ccos_disk_t* disk, const char* path, ccos_inode_t* dir, const char* destpath);

/**
 * @brief      Dumps all files and directories from CCOS disk image to a custom folder.
 *
 * @param[in]  disk      Compass disk image.
 * @param[in]  path      The path to CCOS image.
 * @param[in]  destpath  The path to destination folder.
 *
 * @return     0 on success, -1 otherwise.
 */
int dump_image_to(ccos_disk_t* disk, const char* path, const char* destpath);

/**
 * @brief      Prints a CCOS image contents.
 *
 * @param[in]  disk          Compass disk image.
 * @param[in]  path          The path to CCOS image.
 * @param[in]  short_format  Use shorter, 80-column compatible output format.
 *
 * @return     0 on success, -1 otherwise.
 */
int print_image_info(ccos_disk_t* disk, const char* path, int short_format);

/**
 * @brief      Replace file in the CCOS image.
 *
 * @param[in]  disk         Compass disk image.
 * @param[in]  path         Path to the CCOS image.
 * @param[in]  filename     Path to the file to replace in the CCOS image.
 * @param[in]  target_name  If given, then the file with this name will be replaced in the CCOS image. Otherwise,
 * basename of the filename will be used.
 * @param[in]  in_place     Flag indicating whether to save replaced file under a new name (path.new), or overwrite the
 * original file.
 *
 * @return     0 on success, -1 otherwise.
 */
int replace_file(ccos_disk_t* disk, const char* path, const char* filename, const char* target_name, int in_place);

/**
 * @brief      Copy file from one image into another.
 *
 * @param[in]  src           Source CCOS disk.
 * @param[in]  target_image  Path to the image to copy file to.
 * @param[in]  filename      The name of file to copy.
 * @param[in]  in_place      If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int copy_file(ccos_disk_t* src, const char* target_image, const char* filename, int in_place);

/**
 * @brief      Delete file in the image.
 *
 * @param[in]  disk      Compass disk image.
 * @param[in]  path      Path to the image to delete file in.
 * @param[in]  filename  The name of file to delete.
 * @param[in]  in_place  If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int delete_file(ccos_disk_t* disk, const char* path, const char* filename, int in_place);

/**
 * @brief      Add file to the image.
 *
 * @param[in]  disk        Compass disk image.
 * @param[in]  image_path  Path to the image to add file.
 * @param[in]  file_path   The path to file to add.
 * @param[in]  file_name   The name of file to add.
 * @param[in]  in_place    If true, override original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file(ccos_disk_t* disk, const char* image_path, const char* file_path, const char* file_name, int in_place);

/**
 * @brief      Create directory in the image.
 *
 * @param[in]  disk            Compass disk image.
 * @param[in]  path            Path to the image to create dir.
 * @param[in]  directory_name  The name of directory to create.
 * @param[in]  in_place        If true, override original target image. Otherwise, save new image under
 * {target_image}.out name.
 *
 * @return     0 on success, -1 otherwise.
 */
int create_directory(ccos_disk_t* disk, char* path, char* directory_name, int in_place);

/**
 * @brief      Rename file in the image.
 *
 * @param[in]  disk       Compass disk image.
 * @param      path       Path to the image.
 * @param      file_name  The current name of the file.
 * @param      new_name   The new name of the file.
 * @param[in]  in_place   If true, overwrite original target image. Otherwise, save new image under {target_image}.out
 * name.
 *
 * @return     0 on success, -1 otherwise.
 */
int rename_file(ccos_disk_t* disk, char* path, char* file_name, char* new_name, int in_place);

/**
 * @brief      Create new blank CCOS image file.
 *
 * @param[in]  disk  Compass disk image.
 * @param[in]  path  Path where to create the new image file.
 * @param[in]  size  Size of the image in bytes. Must be a multiple of the sector size.
 *
 * @return     0 on success, -1 otherwise.
 */
int create_blank_image(ccos_disk_t* disk, char* path, size_t size);

#endif  // WRAPPER_H
