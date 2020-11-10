//
// Created by kirill on 21.05.2020.
//

#ifndef CCOS_DISK_TOOL_CCOS_PRIVATE_H
#define CCOS_DISK_TOOL_CCOS_PRIVATE_H

#include <ccos_image.h>

#define CCOS_INVALID_BLOCK 0xFFFF

#define CCOS_DIR_ENTRIES_OFFSET 0x1
#define CCOS_DIR_ENTRY_SUFFIX_LENGTH 0x2
#define CCOS_DIR_LAST_ENTRY_MARKER 0xFFU

#define CCOS_EMPTY_BLOCK_MARKER 0xFFFFFFFF

#define CCOS_DATA_OFFSET 0x4
#define CCOS_BLOCK_SUFFIX_LENGTH 0x4
#define CCOS_BLOCK_DATA_SIZE (BLOCK_SIZE - (CCOS_DATA_OFFSET + CCOS_BLOCK_SUFFIX_LENGTH))

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
typedef struct ccos_inode_t_ {
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

#pragma pack(push, 1)
typedef struct {
  uint16_t block;
  uint8_t name_length;
} dir_entry_t;
#pragma pack(pop)

typedef struct {
  uint16_t offset;
  size_t size;
  ccos_inode_t* file;
} parsed_directory_element_t;

/**
 * @brief      Calculate checksum as it done in Compass's BIOS.
 *
 * @param[in]  data       The data.
 * @param[in]  data_size  The data size.
 *
 * @return     Checksum of the data passed.
 */
uint16_t calc_checksum(const uint8_t* data, uint16_t data_size);

/**
 * @brief      Calculate checksum of the file metadata.
 *
 * @param[in]  inode  The file.
 *
 * @return     File metadata checksum.
 */
uint16_t calc_inode_metadata_checksum(const ccos_inode_t* inode);

/**
 * @brief      Calculates the checksum of file blocks section.
 *
 * @param[in]  inode  The file.
 *
 * @return     The file blocks section checksum.
 */
uint16_t calc_inode_blocks_checksum(const ccos_inode_t* inode);

/**
 * @brief      Calculates the checksum of the content inode.
 *
 * @param[in]  content_inode  The content inode.
 *
 * @return     The content inode checksum.
 */
uint16_t calc_content_inode_checksum(const ccos_content_inode_t* content_inode);

/**
 * @brief      Calculates the checksum of image's bitmask.
 *
 * @param[in]  bitmask  CCOS image bitmask.
 *
 * @return     The bitmask checksum.
 */
uint16_t calc_bitmask_checksum(const ccos_bitmask_t* bitmask);

/**
 * @brief      Recalculate checksums of the inode.
 *
 * @param      inode  The inode.
 */
void update_inode_checksums(ccos_inode_t* inode);

/**
 * @brief      Recalculate checksum of the content inode.
 *
 * @param      content_inode  The content inode.
 */
void update_content_inode_checksums(ccos_content_inode_t* content_inode);

/**
 * @brief      Re-calculate and update CCOS image bitmask checksum.
 *
 * @param      bitmask  CCOS image bitmask.
 */
void update_bitmask_checksum(ccos_bitmask_t* bitmask);

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
int get_superblock(const uint8_t* data, size_t image_size, uint16_t* superblock);

/**
 * @brief      Get the CCOS filesystem inode at the given block.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     Pointer to CCOS filesystem inode structure.
 */
ccos_inode_t* get_inode(uint16_t block, const uint8_t* data);

/**
 * @brief      Parse an inode and return the list of the file content blocks.
 *
 * @param[in]  file         Inode first block number.
 * @param[in]  data          CCOS image data.
 * @param      blocks_count  The file content blocks count.
 * @param      blocks        The file content block numbers.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_file_blocks(const ccos_inode_t* file, const uint8_t* data, size_t* blocks_count, uint16_t** blocks);

/**
 * @brief      Find bitmask section in the CCOS image and return it.
 *
 * @param      data       CCOS image data.
 * @param[in]  data_size  Image data size.
 *
 * @return     The bitmask on success, NULL otherwise.
 */
ccos_bitmask_t* get_bitmask(uint8_t* data, size_t data_size);

/**
 * @brief      Find available free block in the image and return it's number.
 *
 * @param[in]  bitmask  CCOS image bitmask.
 *
 * @return     The free block on success, CCOS_INVALID_BLOCK if no free space in the image.
 */
uint16_t get_free_block(const ccos_bitmask_t* bitmask);

/**
 * @brief      Mark block in the bitmask as free or used.
 *
 * @param      bitmask  CCOS image bitmask.
 * @param[in]  block    The number of the block.
 * @param[in]  mode     The mode (0 for free, 1 for used).
 */
void mark_block(ccos_bitmask_t* bitmask, uint16_t block, uint8_t mode);

/**
 * @brief      Initialize inode at the given block.
 *
 * @param[in]  block       The block to create inode at.
 * @param[in]  parent_dir_block  The parent dir block
 * @param      image_data  CCOS image data.
 *
 * @return     Pointer to the newly created inode on success, NULL otherwise.
 */
ccos_inode_t* init_inode(uint16_t block, uint16_t parent_dir_block, uint8_t* image_data);

/**
 * @brief      Adds a new content inode (block with file block data) to the content inode list of the given file.
 *
 * @param      file     The file to add content inode to.
 * @param      data     CCOS image data.
 * @param      bitmask  CCOS image bitmask.
 *
 * @return     New content inode on success, NULL otherwise.
 */
ccos_content_inode_t* add_content_inode(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask);

/**
 * @brief      Get content inode at the given block number.
 *
 * @param[in]  block  Block number.
 * @param[in]  data   CCOS image data.
 *
 * @return     The content inode.
 */
ccos_content_inode_t* get_content_inode(uint16_t block, const uint8_t* data);

/**
 * @brief      Gets the last content inode in the content inode list of the given file.
 *
 * @param[in]  file        The file.
 * @param[in]  image_data  CCOS image data.
 *
 * @return     The last content inode on success, NULL otherwise.
 */
ccos_content_inode_t* get_last_content_inode(const ccos_inode_t* file, const uint8_t* image_data);

/**
 * @brief      Cleanup image block at the given number and mark it as empty both in the image and in the image bitmask.
 *
 * @param[in]  block    Block number.
 * @param      image    CCOS image data.
 * @param      bitmask  CCOS image bitmask.
 */
void erase_block(uint16_t block, uint8_t* image, ccos_bitmask_t* bitmask);

/**
 * @brief      Removes the last content inode from the file's content inodes list, and erases this content inode block.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask  CCOS image bitmask.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_content_inode(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask);

/**
 * @brief      Removes last content block from the file.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask  CCOS image bitmask.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_block_from_file(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask);

/**
 * @brief      Adds content block to the file.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask  CCOS image bitmask.
 *
 * @return     { description_of_the_return_value }
 */
uint16_t add_block_to_file(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask);

/**
 * @brief      Add new file entry to the list of files in the given directory.
 *
 * @param      directory   The directory to add file entry to.
 * @param      file        The file to add to the directory.
 * @param      image_data  CCOS image data.
 * @param[in]  image_size  Image size.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file_to_directory(ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size);

/**
 * @brief      Insert new directory entry into the directory, effectively making this directory a parent for the given
 * file.
 *
 * @param      directory   The directory.
 * @param[in]  image_data  CCOS image data.
 * @param[in]  image_size  CCOS image size.
 * @param      file        The file.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file_entry_to_dir_contents(ccos_inode_t* directory, uint8_t* image_data, size_t image_size, ccos_inode_t* file);

/**
 * @brief      Perse CCOS file name and return it's basename and it's type.
 *
 * @param[in]  file_name    The file name.
 * @param      basename     The basename.
 * @param      type         The file type.
 * @param      name_length  Name length.
 * @param      type_length  File type length.
 *
 * @return     0 on success, -1 otherwise.
 */
int parse_file_name(const short_string_t* file_name, char* basename, char* type, size_t* name_length,
                    size_t* type_length);

/**
 * @brief      Extract list of files stored in the directory by parsing directory raw contents.
 *
 * @param      image_data           CCOS image data.
 * @param[in]  directory_data       Directory raw contents.
 * @param[in]  directory_data_size  Directory contents size.
 * @param[in]  entry_count          Nunber of files in the directory.
 * @param      entries              Array of files located in the directory.
 *
 * @return     0 on success, -1 otherwise.
 */
int parse_directory_data(uint8_t* image_data, const uint8_t* directory_data, size_t directory_data_size,
                         uint16_t entry_count, parsed_directory_element_t** entries);

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
int get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size);

/**
 * @brief      Return info about free blocks in a CCOS image.
 *
 * @param[in]  bitmask            Image bitmask.
 * @param[in]  data_size          Image size.
 * @param      free_blocks_count  Pointer to free blocks count.
 * @param      free_blocks        Pointer to the free blocks array.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_free_blocks(ccos_bitmask_t* bitmask, size_t data_size, size_t* free_blocks_count, uint16_t** free_blocks);

int find_file_index_in_directory_data(ccos_inode_t* file, ccos_inode_t* directory,
                                      parsed_directory_element_t* elements);

/**
 * @brief      Checks if the directory is root.
 *
 * @param      file   The directory.
 *
 * @return     1 if directory is root, 0 otherwise.
 */
int is_root_dir(ccos_inode_t* file);

#endif  // CCOS_DISK_TOOL_CCOS_PRIVATE_H
