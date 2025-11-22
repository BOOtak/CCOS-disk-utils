#ifndef CCOS_IMAGE_H
#define CCOS_IMAGE_H

#include "ccos_structure.h"
#include "string_utils.h"

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} version_t;

typedef struct ccos_inode_t_ ccos_inode_t;

typedef enum { UNKNOWN, DATA, EMPTY } block_type_t;

/**
 * @brief      Checking image for validity.
 *
 * @param[in]  file_data  The file.
 *
 * @return     0 if valid, -1 otherwise.
 */
int ccos_check_image(const uint8_t* file_data);

/**
 * @brief      Get the file ID.
 *
 * @param[in]  inode  The file.
 *
 * @return     The ID of the file.
 */
uint16_t ccos_file_id(const ccos_inode_t* inode);

/**
 * @brief      Get the file version.
 *
 * @param[in]  file   The file.
 *
 * @return     The version of the file.
 */
version_t ccos_get_file_version(const ccos_inode_t* file);

/**
 * @brief      Set the file version.
 *
 * @param[in]  ctx           Filesystem context handle.
 * @param[in]  file          The file.
 * @param[in]  new_version   The new version to set.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_set_file_version(ccfs_handle ctx, ccos_inode_t* file, version_t new_version);

/**
 * @brief      Get the name of the file.
 *
 * @param[in]  file   File inode.
 *
 * @return     Pointer to short string with a file name.
 */
short_string_t* ccos_get_file_name(const ccos_inode_t* file);

/**
 * @brief      Get root directory of the given image.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param      data       CCOS image data.
 * @param[in]  data_size  Image data size.
 *
 * @return     Root directory on success, NULL otherwise.
 */
ccos_inode_t* ccos_get_root_dir(ccfs_handle ctx, uint8_t* data, size_t data_size);

/**
 * @brief      Read contents from the directory inode.
 *
 * @param[in]  ctx             Filesystem context handle.
 * @param[in]  dir             Directory inode.
 * @param[in]  data            CCOS image data.
 * @param      entry_count     Count of the items in the directory.
 * @param      entries         Directory contents inodes.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_dir_contents(ccfs_handle ctx, ccos_inode_t* dir, uint8_t* data, uint16_t* entry_count, ccos_inode_t*** entries);

/**
 * @brief      Determine whether the given inode is a directory's inode.
 *
 * @param[in]  file  The file to check.
 *
 * @return     1 if the given inode belongs to a directory, 0 otherwise.
 */
int ccos_is_dir(const ccos_inode_t* file);

/**
 * @brief      Get the size of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The size of a file at a given inode.
 */
uint32_t ccos_get_file_size(const ccos_inode_t* file);

/**
 * @brief      Get the creation date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The creation date of a file at a given inode.
 */
ccos_date_t ccos_get_creation_date(const ccos_inode_t* file);

/**
 * @brief      Get the modification date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The modification date of a file at a given inode.
 */
ccos_date_t ccos_get_mod_date(const ccos_inode_t* file);

/**
 * @brief      Get the expiry date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The expiry date of a file at a given inode.
 */
ccos_date_t ccos_get_exp_date(const ccos_inode_t* file);

/**
 * @brief      Changes the creation date of a file or folder.
 *
 * @param[in]  ctx       Filesystem context handle.
 * @param      file      The file or the directory.
 * @param      new_date  The new date variable.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_set_creation_date(ccfs_handle ctx, ccos_inode_t* file, ccos_date_t new_date);

/**
 * @brief      Changes the modification date of a file or folder.
 *
 * @param[in]  ctx       Filesystem context handle.
 * @param      file      The file or the directory.
 * @param      new_date  The new date variable.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_set_mod_date(ccfs_handle ctx, ccos_inode_t* file, ccos_date_t new_date);

/**
 * @brief      Changes the expiration date of a file or folder.
 *
 * @param[in]  ctx       Filesystem context handle.
 * @param      file      The file or the directory.
 * @param      new_date  The new date variable.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_set_exp_date(ccfs_handle ctx, ccos_inode_t* file, ccos_date_t new_date);

/**
 * @brief      Replace file in the CCOS image data.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param[in]  file        The file to replace.
 * @param[in]  file_data   The new file contents.
 * @param[in]  file_size   The new file size (it should match old file size).
 * @param      image_data  CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_replace_file(ccfs_handle ctx, ccos_inode_t* file, const uint8_t* file_data, uint32_t file_size, uint8_t* image_data);

/**
 * @brief      Get info about blocks in the image. Traverse all blocks in the CCOS image and return array filled with
 * corresponding block types.
 *
 * @param[in]  ctx                Filesystem context handle.
 * @param[in]  data               CCOS image data.
 * @param[in]  data_size          The data size.
 * @param      image_map          Block types array.
 * @param      free_blocks_count  The free blocks count.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_image_map(ccfs_handle ctx, const uint8_t* data, size_t data_size, block_type_t** image_map, size_t* free_blocks_count);

/**
 * @brief      Read file contents into memory buffer.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param[in]  file        File to read.
 * @param[in]  image_data  CCOS image data.
 * @param      file_data   The file data.
 * @param      file_size   The file size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_read_file(ccfs_handle ctx, ccos_inode_t* file, const uint8_t* image_data, uint8_t** file_data, size_t* file_size);

/**
 * @brief      Get parent directory of the given file.
 *
 * @param[in]  ctx   Filesystem context handle.
 * @param      file  The file.
 * @param      data  CCOS image data.
 *
 * @return     Parent directory on success, NULL otherwise.
 */
ccos_inode_t* ccos_get_parent_dir(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data);

/**
 * @brief      Copy file from one CCOS image into another.
 *
 * @param[in]  ctx              Filesystem context handle.
 * @param      dest_image       The destination CCOS image.
 * @param[in]  dest_image_size  The destination image size.
 * @param      dest_directory   The directory in the destination image to copy file to.
 * @param[in]  src_image        The source CCOS image.
 * @param[in]  src_file         The source file.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_copy_file(ccfs_handle ctx, uint8_t* dest_image, size_t dest_image_size, ccos_inode_t* dest_directory,
                   const uint8_t* src_image, ccos_inode_t* src_file);

/**
 * @brief      Delete file in the image.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param[in]  image_data  CCOS image data.
 * @param[in]  data_size   The image size.
 * @param[in]  file        The file to delete.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_delete_file(ccfs_handle ctx, uint8_t* image_data, size_t data_size, ccos_inode_t* file);

/**
 * @brief      Add new file to the given directory.
 *
 * @param[in]  ctx             Filesystem context handle.
 * @param      dest_directory  The destination directory to add file to.
 * @param[in]  file_data       File data.
 * @param[in]  file_size       File data size.
 * @param[in]  file_name       File name.
 * @param      image_data      CCOS image data.
 * @param[in]  image_size      Image size.
 *
 * @return     Newly created file inode on success, NULL otherwise.
 */
ccos_inode_t* ccos_add_file(ccfs_handle ctx, ccos_inode_t* dest_directory, uint8_t* file_data, size_t file_size, const char* file_name,
                            uint8_t* image_data, size_t image_size);

/**
 * @brief      Check file checksums and file structure, log to stderr in case of malformed file.
 *
 * @param[in]  ctx   Filesystem context handle.
 * @param[in]  file  File to check.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_validate_file(ccfs_handle ctx, const ccos_inode_t* file);

/**
 * @brief      Return amount of free space available in the image.
 *
 * @param[in]  ctx        Filesystem context handle.
 * @param      data       CCOS image data.
 * @param[in]  data_size  Data size.
 *
 * @return     Free space in the image, in bytes.
 */
size_t ccos_calc_free_space(ccfs_handle ctx, uint8_t* data, size_t data_size);

/**
 * @brief      Overwrite file contents with the given data.
 *
 * @param[in]  ctx         Filesystem context handle.
 * @param      file        The file.
 * @param      image_data  CCOS image data.
 * @param[in]  image_size  Image data size.
 * @param[in]  file_data   File contents.
 * @param[in]  file_size   File contents size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_write_file(ccfs_handle ctx, ccos_inode_t* file, uint8_t* image_data, size_t image_size,
                    const uint8_t* file_data, size_t file_size);

/**
 * @brief      Get info about the name of the given file.
 *
 * @param[in]  inode        File to parse name of.
 * @param      basename     File basename (optional, may be NULL).
 * @param      type         File extension (optional, may be NULL).
 * @param      name_length  Length of the name (optional, may be NULL).
 * @param      type_length  The type length (optional, may be NULL).
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_parse_file_name(const ccos_inode_t* inode, char* basename, char* type, size_t* name_length, size_t* type_length);

/**
 * @brief      Create directory in the image.
 *
 * @param[in]  ctx             Filesystem context handle.
 * @param      parent_dir      The parent directory.
 * @param[in]  directory_name  New directory name.
 * @param      image_data      CCOS image data.
 * @param[in]  image_size      CCOS image size.
 *
 * @return     Newly created directory inode on success, NULL otherwise.
 */
ccos_inode_t* ccos_create_dir(ccfs_handle ctx, ccos_inode_t* parent_dir, const char* directory_name,
                              uint8_t* image_data, size_t image_size);

/**
 * @brief      Rename file and change type.
 *
 * @param[in]  ctx             Filesystem context handle.
 * @param      image_data      CCCOS image data.
 * @param[in]  image_size      CCOS image size.
 * @param      file            The file.
 * @param[in]  new_name        The new name.
 * @param[in]  new_type        The new type (optional, may be NULL, if you don't want to change it).
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_rename_file(ccfs_handle ctx, uint8_t* image_data, size_t image_size, ccos_inode_t* file,
                     const char* new_name, const char* new_type);

/**
 * @brief      Get label of provided image and return it.
 *
 * @param[in]  ctx                Filesystem context handle.
 * @param[in]  data               CCOS image data.
 * @param[in]  data_size          The data size.
 *
 * @return     Image label in char*
 */
char* ccos_get_image_label(ccfs_handle ctx, uint8_t* data, size_t data_size);

/**
 * @brief      Set label of provided image.
 *
 * @param[in]  ctx                Filesystem context handle.
 * @param[in]  data               CCOS image data.
 * @param[in]  data_size          The data size.
 * @param[in]  label              The new label.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_set_image_label(ccfs_handle ctx, uint8_t* data, size_t data_size, const char* label);

/**
 * @brief      Create new empty image.
 *
 * @param[in]  ctx     Filesystem context handle.
 * @param[in]  blocks  Number of blocks in the image.
 *
 * @return     Image data on success, NULL otherwise.
 */
uint8_t* ccos_create_new_image(ccfs_handle ctx, size_t blocks);

#endif  // CCOS_IMAGE_H
