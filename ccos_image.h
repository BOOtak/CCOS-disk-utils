#ifndef CCOS_IMAGE_H
#define CCOS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "string_utils.h"

#define BLOCK_SIZE 512

#define CCOS_MAX_FILE_NAME 80
#define MAX_BLOCKS_IN_INODE 146
#define MAX_BLOCKS_IN_CONTENT_INODE 246
#define BITMASK_SIZE 500

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} version_t;

#pragma pack(push, 1)
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t tenthOfSec;
  uint8_t dayOfWeek;
  uint16_t dayOfYear;
} ccos_date_t;
#pragma pack(pop)

typedef struct {
  uint16_t file_id;
  uint16_t file_fragment_index;
} ccos_block_header_t;

#pragma pack(push, 1)
typedef struct {
  ccos_block_header_t header;
  uint16_t blocks_checksum;  // checksum([block_next ... block_end), file_id, file_fragment_index)
  uint16_t block_next;       // next block with content blocks. 0xFFFF if not present
  uint16_t block_current;    // current block with content blocks
  uint16_t block_prev;       // previous block with content blocks. 0xFFFF if not present
} ccos_block_data_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  ccos_block_header_t header;
  uint32_t file_size;
  uint8_t name_length;
  char name[CCOS_MAX_FILE_NAME];
  ccos_date_t creation_date;
  uint16_t dir_file_id;  // file id of the parent directory
  ccos_date_t mod_date;
  ccos_date_t expiration_date;
  uint32_t machine_ID;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t comp;         // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t encry;        // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t protec;       // write-protected ???
  uint8_t pswd_len;     // unused in 3.0+ ???
  char pswd[4];         // unused in 3.0+ ???
  uint32_t dir_length;  // directory size in bytes. Matches file_size for large dirs, but not always
  uint16_t dir_count;   // number of files in the directory
  uint8_t pad[6];
  uint8_t asc;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t uses_8087;
  uint8_t version_major;
  uint8_t version_minor;
  uint32_t system;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t pad2[11];
  uint8_t version_patch;
  uint32_t prop_length;  // indicates how much bytes at the beginning of the file are used to store some properties and
                         // are not part of the file
  uint8_t rom;           // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t rom_id;       // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t mode;         // from InteGRiD Sources OSINCS/WSTYPE.INC
  char RDB[3];           // from InteGRiD Sources OSINCS/WSTYPE.INC
  char UDB[20];          // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t grid_central_use;   // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t metadata_checksum;  // checksum([file_id ... metadata_checksum))
  ccos_block_data_t content_inode_info;
  uint16_t content_blocks[MAX_BLOCKS_IN_INODE];
  uint32_t block_end;
} ccos_inode_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  ccos_block_data_t content_inode_info;
  uint16_t content_blocks[MAX_BLOCKS_IN_CONTENT_INODE];
  uint64_t block_end;
} ccos_content_inode_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  ccos_block_header_t header;
  uint16_t checksum;
  uint16_t allocated;
  uint8_t bytes[BITMASK_SIZE];
  uint32_t block_end;
} ccos_bitmask_t;
#pragma pack(pop)

typedef enum { UNKNOWN, DATA, EMPTY } block_type_t;

int ccos_check_image(const uint8_t* file_data);

/**
 * @brief      Get the file version.
 *
 * @param[in]  file   The file.
 * @param[in]  data   CCOS image data.
 *
 * @return     The version of the file.
 */
version_t ccos_get_file_version(ccos_inode_t* file);

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
 * @param      data       CCOS image data.
 * @param[in]  data_size  Image data size.
 *
 * @return     Root directory on success, NULL otherwise.
 */
ccos_inode_t* ccos_get_root_dir(uint8_t* data, size_t data_size);

/**
 * @brief      Read contents from the directory inode.
 *
 * @param[in]  dir             Directory inode.
 * @param[in]  data            CCOS image data.
 * @param      entry_count     Count of the items in the directory.
 * @param      entries         Directory contents inodes.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_dir_contents(const ccos_inode_t* dir, uint8_t* data, uint16_t* entry_count, ccos_inode_t*** entries);

/**
 * @brief      Determine whether the given inode is a directory's inode.
 *
 * @param[in]  file  The file to check.
 *
 * @return     1 if the given inode belongs to a directory, 0 otherwise.
 */
int ccos_is_dir(ccos_inode_t* file);

/**
 * @brief      Get the size of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The size of a file at a given inode.
 */
uint32_t ccos_get_file_size(ccos_inode_t* file);

/**
 * @brief      Get the creation date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The creation date of a file at a given inode.
 */
ccos_date_t ccos_get_creation_date(ccos_inode_t* file);

/**
 * @brief      Get the modification date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The modification date of a file at a given inode.
 */
ccos_date_t ccos_get_mod_date(ccos_inode_t* file);

/**
 * @brief      Get the expiry date of a file at a given inode.
 *
 * @param[in]  file  The file.
 *
 * @return     The expiry date of a file at a given inode.
 */
ccos_date_t ccos_get_exp_date(ccos_inode_t* file);

/**
 * @brief      Replace file in the CCOS image data.
 *
 * @param[in]  file        The file to replace.
 * @param[in]  file_data   The new file contents.
 * @param[in]  file_size   The new file size (it should match old file size).
 * @param      image_data  CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_replace_file(ccos_inode_t* file, const uint8_t* file_data, uint32_t file_size, uint8_t* image_data);

/**
 * @brief      Get info about blocks in the image. Traverse all blocks in the CCOS image and return array filled with
 * corresponding block types.
 *
 * @param[in]  data               CCOS image data.
 * @param[in]  data_size          The data size.
 * @param      image_map          Block types array.
 * @param      free_blocks_count  The free blocks count.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_image_map(const uint8_t* data, size_t data_size, block_type_t** image_map, size_t* free_blocks_count);

/**
 * @brief      Read file contents into memory buffer
 *
 * @param[in]  file        File to read.
 * @param[in]  image_data  CCOS image data.
 * @param      file_data   The file data.
 * @param      file_size   The file size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_read_file(ccos_inode_t* file, const uint8_t* image_data, uint8_t** file_data, size_t* file_size);

/**
 * @brief      Get parent directory of the given file.
 *
 * @param      file  The file.
 * @param      data  CCOS image data.
 *
 * @return     Parent directory on success, NULL otherwise.
 */
ccos_inode_t* ccos_get_parent_dir(ccos_inode_t* file, uint8_t* data);

/**
 * @brief      Copy file from one CCOS image into another.
 *
 * @param      dest_image       The destination CCOS image.
 * @param[in]  dest_image_size  The destination image size.
 * @param      dest_directory   The directory in the destination image to copy file to.
 * @param[in]  src_image        The source CCOS image.
 * @param[in]  src_file         The source file.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_copy_file(uint8_t* dest_image, size_t dest_image_size, ccos_inode_t* dest_directory, const uint8_t* src_image,
                   ccos_inode_t* src_file);

/**
 * @brief      Delete file in the image.
 *
 * @param      data       CCOS image data.
 * @param[in]  data_size  The image size.
 * @param[in]  file        The file to delete.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_delete_file(uint8_t* data, size_t data_size, ccos_inode_t* file);

/**
 * @brief      Add new file to the given directory.
 *
 * @param[in]  dest_directory  The destination directory to add file to.
 * @param[in]  file_data       File data.
 * @param[in]  file_size       File data size.
 * @param[in]  file_name       File name.
 * @param      image_data      CCOS image data.
 * @param[in]  image_size      Image size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_add_file(ccos_inode_t* dest_directory, uint8_t* file_data, size_t file_size, const char* file_name,
                  uint8_t* image_data, size_t image_size);

/**
 * @brief      Check file checksums, log to stderr in case of checksum mismatch.
 *
 * @param[in]  file  File to check.
 */
void ccos_check_file_checksum(const ccos_inode_t* file);

/**
 * @brief      Return amount of free space available in the image.
 *
 * @param      data       CCOS image data.
 * @param[in]  data_size  Data size.
 *
 * @return     Free space in the image, in bytes.
 */
size_t ccos_calc_free_space(uint8_t* data, size_t data_size);

/**
 * @brief      Overwrite file contents with the given data.
 *
 * @param      file        The file.
 * @param      image_data  CCOS image data.
 * @param[in]  image_size  Image data size.
 * @param[in]  file_data   File contents.
 * @param[in]  file_size   File contents size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_write_file(ccos_inode_t* file, uint8_t* image_data, size_t image_size, const uint8_t* file_data,
                    size_t file_size);

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
int ccos_parse_file_name(ccos_inode_t* inode, char* basename, char* type, size_t* name_length, size_t* type_length);

//TODO: add new directory; remove directory recursively

#endif  // CCOS_IMAGE_H
