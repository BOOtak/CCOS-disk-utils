#ifndef CCOS_IMAGE_H
#define CCOS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 512

#define CCOS_MAX_FILE_NAME 80
#define MAX_BLOCKS_IN_INODE 146
#define MAX_BLOCKS_IN_CONTENT_INODE 246

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} version_t;

struct short_string_t_;
typedef struct short_string_t_ short_string_t;

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

typedef enum { UNKNOWN, DATA, EMPTY } block_type_t;

/**
 * @brief      Calculate checksum as it done in Compass's BIOS.
 *
 * @param[in]  data       The data.
 * @param[in]  data_size  The data size.
 *
 * @return     Checksum of the data passed.
 */
uint16_t ccos_make_checksum(const uint8_t* data, uint16_t data_size);

uint16_t ccos_make_inode_metadata_checksum(const ccos_inode_t* inode);

uint16_t ccos_make_inode_blocks_checksum(const ccos_inode_t* inode);

uint16_t ccos_make_content_inode_checksum(const ccos_content_inode_t* content_inode);

/**
 * @brief      Find a superblock (i.e. the inode with the root directory description) in a CCOS filesystem image.
 *
 * @param[in]  data        CCOS image data.
 * @param[in]  image_size  The image size.
 * @param      superblock  The superblock to return.
 *
 * @return     0 on success, with superblock numper passed out to the superblock parameter, -1 on error (i.e. in case of
 * invalid image format).
 */
int ccos_get_superblock(const uint8_t* data, size_t image_size, uint16_t* superblock);

/**
 * @brief      Get the CCOS filesystem inode at the given block.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     Pointer to CCOS filesystem inode structure.
 */
const ccos_inode_t* ccos_get_inode(uint16_t block, const uint8_t* data);

/**
 * @brief      Get the file version.
 *
 * @param[in]  block  The block number of the file inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The version of the file.
 */
version_t ccos_get_file_version(uint16_t block, const uint8_t* data);

/**
 * @brief      Get the name of the file.
 *
 * @param[in]  block  The block number of the file inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     Pointer to short string with a file name.
 */
const short_string_t* ccos_get_file_name(uint16_t block, const uint8_t* data);

/**
 * @brief      Convert the string from the internal short string format into C string.
 *
 * @param[in]  short_string  The short string.
 *
 * @return     Pointer to allocated C string on success, NULL otherwise.
 */
char* ccos_short_string_to_string(const short_string_t* short_string);

/**
 * @brief      Parse an inode and return the list of the file content blocks.
 *
 * @param[in]  block         Inode first block number.
 * @param[in]  data          CCOS image data.
 * @param      blocks_count  The file content blocks count.
 * @param      blocks        The file content block numbers.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_file_blocks(uint16_t block, const uint8_t* data, size_t* blocks_count, uint16_t** blocks);

/**
 * @brief      Read contents from the directory inode.
 *
 * @param[in]  block           The block number of the directory inode.
 * @param[in]  data            CCOS image data.
 * @param      entry_count     Count of the items in the directory.
 * @param      entries_blocks  Directory contents inodes.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_dir_contents(uint16_t block, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_inodes);

/**
 * @brief      Determine whether the given inode is a directory's inode.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     1 if the given inode belongs to a directory, 0 otherwise.
 */
int ccos_is_dir(uint16_t block, const uint8_t* data);

/**
 * @brief      Read raw data from the image at a given block. Notice it won't allocate any memory, just return a pointer
 * and a size of a raw data inside a block.
 *
 * @param[in]  block  Block number.
 * @param[in]  data   CCOS image data.
 * @param      start  Start address of the raw data.
 * @param      size   The size of a raw data.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_block_data(uint16_t block, const uint8_t* data, uint8_t** start, size_t* size);

/**
 * @brief      Get the size of a file at a given inode.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The size of a file at a given inode.
 */
uint32_t ccos_get_file_size(uint16_t block, const uint8_t* data);

/**
 * @brief      Get the creation date of a file at a given inode.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The creation date of a file at a given inode.
 */
ccos_date_t ccos_get_creation_date(uint16_t block, const uint8_t* data);

/**
 * @brief      Get the modification date of a file at a given inode.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The modification date of a file at a given inode.
 */
ccos_date_t ccos_get_mod_date(uint16_t block, const uint8_t* data);

/**
 * @brief      Get the expiry date of a file at a given inode.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The expiry date of a file at a given inode.
 */
ccos_date_t ccos_get_exp_date(uint16_t block, const uint8_t* data);

/**
 * @brief      Perse CCOS file name and return it's basename and it's type.
 *
 * @param[in]  file_name  The file name.
 * @param      basename   The basename.
 * @param      type       The file type.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_parse_file_name(const short_string_t* file_name, char* basename, char* type);

/**
 * @brief      Replace file in the CCOS image data.
 *
 * @param[in]  block       The block number of the inode of the file to replace.
 * @param[in]  file_data   The new file contents.
 * @param[in]  file_size   The new file size (it should match old file size).
 * @param      image_data  CCOS image data.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_replace_file(uint16_t block, const uint8_t* file_data, uint32_t file_size, uint8_t* image_data);

int ccos_get_image_map(const uint8_t* data, size_t data_size, block_type_t** image_map, size_t* free_blocks_count);

/**
 * @brief      Recalculate checksums of the inode.
 *
 * @param      inode  The inode.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_update_checksums(ccos_inode_t* inode);

/**
 * @brief      Recalculate checksum of the content inode.
 *
 * @param      content_inode  The content inode.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_update_content_inode_checksums(ccos_content_inode_t* content_inode);

/**
 * @brief      Create empty inode at the given block.
 *
 * @param[in]  block       The block to create inode at.
 * @param[in]  parent_dir_block  The parent dir block
 * @param      image_data  CCOS image data.
 *
 * @return     Pointer to the newly created inode on success, NULL otherwise.
 */
ccos_inode_t* ccos_create_inode(uint16_t block, uint16_t parent_dir_block, uint8_t* image_data);

/**
 * @brief      Append new content inode to the current inode.
 *
 * @param      inode  The inode to append content inode to.
 * @param[in]  block  The block to create new content inode at.
 * @param      image_data  CCOS image data.
 *
 * @return     Pointer to the newly created content inode on success, NULL otherwise.
 */
ccos_content_inode_t* ccos_append_content_inode(ccos_inode_t* inode, uint16_t block, uint8_t* image_data);

/**
 * @brief      Add file to a CCOS image.
 *
 * @param      image_data  CCOS image data.
 * @param      directory   The directory in which this file will be created.
 * @param      file_data   The file data.
 * @param[in]  file_size   The file size.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_create_file(uint8_t* image_data, ccos_inode_t* directory, uint8_t* file_data, uint32_t file_size);

int ccos_get_free_blocks(const uint8_t* data, size_t data_size, uint16_t** free_blocks, size_t* free_blocks_count);

int ccos_add_file_to_directory(ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size);

int ccos_copy_file(uint8_t* dest_image, size_t dest_image_size, ccos_inode_t* dest_directory, const uint8_t* src_image,
                   const ccos_inode_t* src_file);

int ccos_delete_file(uint8_t* image, size_t image_size, const ccos_inode_t* file);

#endif  // CCOS_IMAGE_H
