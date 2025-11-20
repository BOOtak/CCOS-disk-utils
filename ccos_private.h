//
// Created by kirill on 21.05.2020.
//

#ifndef CCOS_DISK_TOOL_CCOS_PRIVATE_H
#define CCOS_DISK_TOOL_CCOS_PRIVATE_H

#include "ccos_context.h"
#include "ccos_structure.h"
#include "string_utils.h"

typedef struct {
  uint16_t offset;
  size_t size;
  ccos_inode_t* file;
} parsed_directory_element_t;

typedef enum { CREATED, MODIF, EXPIR } date_type_t;

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
uint16_t calc_inode_blocks_checksum(ccfs_handle ctx, const ccos_inode_t* inode);

/**
 * @brief      Calculates the checksum of the content inode.
 *
 * @param[in]  content_inode  The content inode.
 *
 * @return     The content inode checksum.
 */
uint16_t calc_content_inode_checksum(ccfs_handle ctx, const ccos_content_inode_t* content_inode);

/**
 * @brief      Calculates the checksum of image's bitmask.
 *
 * @param[in]  bitmask  CCOS image bitmask.
 *
 * @return     The bitmask checksum.
 */
uint16_t calc_bitmask_checksum(ccfs_handle ctx, const ccos_bitmask_t* bitmask);

/**
 * @brief      Recalculate checksums of the inode.
 *
 * @param      inode  The inode.
 */
void update_inode_checksums(ccfs_handle ctx, ccos_inode_t* inode);

/**
 * @brief      Recalculate checksum of the content inode.
 *
 * @param      content_inode  The content inode.
 */
void update_content_inode_checksums(ccfs_handle ctx, ccos_content_inode_t* content_inode);

/**
 * @brief      Re-calculate and update CCOS image bitmask checksum.
 *
 * @param      bitmask  CCOS image bitmask.
 */
void update_bitmask_checksum(ccfs_handle ctx, ccos_bitmask_t* bitmask);

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
int get_superblock(ccfs_handle ctx, const uint8_t* data, size_t image_size, uint16_t* superblock);

/**
 * @brief      Get the CCOS filesystem inode at the given block.
 *
 * @param[in]  block  The block number of the inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     Pointer to CCOS filesystem inode structure.
 */
ccos_inode_t* get_inode(ccfs_handle ctx, uint16_t block, const uint8_t* data);

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
int get_file_blocks(ccfs_handle ctx, ccos_inode_t* file, const uint8_t* data, size_t* blocks_count, uint16_t** blocks);

/**
 * @brief Get all bitmask blocks from the image.
 *
 * @param data
 * @param data_size
 * @return ccos_bitmask_list_t
 */
ccos_bitmask_list_t find_bitmask_blocks(ccfs_handle ctx, uint8_t* data, size_t data_size);

/**
 * @brief      Find available free block in the image and return it's number.
 *
 * @param[in]  bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     The free block on success, CCOS_INVALID_BLOCK if no free space in the image.
 */
uint16_t get_free_block(ccfs_handle ctx, const ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Mark block in the bitmask as free or used.
 *
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 * @param[in]  block    The number of the block.
 * @param[in]  mode     The mode (0 for free, 1 for used).
 */
void mark_block(ccfs_handle ctx, ccos_bitmask_list_t* bitmask_list, uint16_t block, uint8_t mode);

/**
 * @brief      Initialize inode at the given block.
 *
 * @param[in]  block       The block to create inode at.
 * @param[in]  parent_dir_block  The parent dir block
 * @param      image_data  CCOS image data.
 *
 * @return     Pointer to the newly created inode on success, NULL otherwise.
 */
ccos_inode_t* init_inode(ccfs_handle ctx, uint16_t block, uint16_t parent_dir_block, uint8_t* image_data);

/**
 * @brief      Adds a new content inode (block with file block data) to the content inode list of the given file.
 *
 * @param      file     The file to add content inode to.
 * @param      data     CCOS image data.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     New content inode on success, NULL otherwise.
 */
ccos_content_inode_t* add_content_inode(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Get content inode at the given block number.
 *
 * @param[in]  block  Block number.
 * @param[in]  data   CCOS image data.
 *
 * @return     The content inode.
 */
ccos_content_inode_t* get_content_inode(ccfs_handle ctx, uint16_t block, const uint8_t* data);

/**
 * @brief      Gets the last content inode in the content inode list of the given file.
 *
 * @param[in]  file        The file.
 * @param[in]  image_data  CCOS image data.
 *
 * @return     The last content inode on success, NULL otherwise.
 */
ccos_content_inode_t* get_last_content_inode(ccfs_handle ctx, const ccos_inode_t* file, const uint8_t* image_data);

/**
 * @brief      Cleanup image block at the given number and mark it as empty both in the image and in the image bitmask.
 *
 * @param[in]  block    Block number.
 * @param      image    CCOS image data.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 */
void erase_block(ccfs_handle ctx, uint16_t block, uint8_t* image, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Removes the last content inode from the file's content inodes list, and erases this content inode block.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_content_inode(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Removes last content block from the file.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_block_from_file(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Adds content block to the file.
 *
 * @param      file     The file.
 * @param      data     CCOS image data.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     { description_of_the_return_value }
 */
uint16_t add_block_to_file(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list);

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
int add_file_to_directory(ccfs_handle ctx, ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size);

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
int add_file_entry_to_dir_contents(ccfs_handle ctx, ccos_inode_t* directory,
                                   uint8_t* image_data, size_t image_size,
                                   ccos_inode_t* file);


/**
 * @brief      Delete file entry from the parent directory.
 *
 * @param      file        The file.
 * @param[in]  image_data  CCOS image data.
 * @param[in]  image_size  CCOS image size.
 *
 * @return     0 on success, -1 otherwise.
 */
int delete_file_from_parent_dir(ccfs_handle ctx, ccos_inode_t* file, uint8_t* image_data, size_t image_size);

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
 * @param[in]  entry_count          Number of files in the directory.
 * @param      entries              Array of files located in the directory.
 *
 * @return     0 on success, -1 otherwise.
 */
int parse_directory_data(ccfs_handle ctx, uint8_t* image_data,
                         const uint8_t* directory_data, size_t directory_data_size,
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
int get_block_data(ccfs_handle ctx, uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size);

/**
 * @brief      Return info about free blocks in a CCOS image.
 *
 * @param[in]  bitmask_list       List of CCOS image bitmask blocks.
 * @param[in]  data_size          Image size.
 * @param      free_blocks_count  Pointer to free blocks count.
 * @param      free_blocks        Pointer to the free blocks array.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_free_blocks(ccfs_handle ctx, ccos_bitmask_list_t* bitmask_list, size_t data_size, size_t* free_blocks_count,
                    uint16_t** free_blocks);

int find_file_index_in_directory_data(ccos_inode_t* file, ccos_inode_t* directory,
                                      parsed_directory_element_t* elements);

/**
 * @brief      Checks if the directory is root.
 *
 * @param      file   The directory.
 *
 * @return     1 if directory is root, 0 otherwise.
 */
int is_root_dir(const ccos_inode_t* file);

/**
 * @brief      Сhanges the date of a file or folder.
 *
 * @param      file      The file or the directory.
 * @param      new_date  The new date variable.
 * @param      type      Date type to replace (CREATED - creation, MODIF - modification, EXPIR - expiration).
 *
 * @return     0 on success, -1 otherwise.
 */
int change_date(ccfs_handle ctx, ccos_inode_t* file, ccos_date_t new_date, date_type_t type);

/**
 * @brief       Create CCOS filesystem in the given image.
 *
 * @param       image_data    CCOS image data.
 * @param       image_size    Image size
 *
 * @return      0 on success, -1 otherwise
 */
int format_image(ccfs_handle ctx, uint8_t* data, size_t image_size);

#endif  // CCOS_DISK_TOOL_CCOS_PRIVATE_H
