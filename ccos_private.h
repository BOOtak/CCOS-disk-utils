//
// Created by kirill on 21.05.2020.
//

#ifndef CCOS_DISK_TOOL_CCOS_PRIVATE_H
#define CCOS_DISK_TOOL_CCOS_PRIVATE_H

#include "ccos_disk.h"
#include "ccos_structure.h"
#include "ccos_string.h"

#define DEFAULT_SUPERBLOCK           0x121
#define DEFAULT_HDD_SUPERBLOCK       0x2420
#define DEFAULT_BUBBLE_SUPERBLOCK    0x3fe
#define CCOS_SUPERBLOCK_ADDR_OFFSET  0x20

#define DEFAULT_BITMASK_BLOCK_ID         (DEFAULT_SUPERBLOCK - 1)
#define DEFAULT_BUBBLE_BITMASK_BLOCK_ID  (DEFAULT_BUBBLE_SUPERBLOCK - 1)
#define CCOS_BITMASK_ADDR_OFFSET         0x1E

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
 * @param[in]  disk   Compass disk image.
 * @param[in]  inode  The file.
 *
 * @return     The file blocks section checksum.
 */
uint16_t calc_inode_blocks_checksum(ccos_disk_t* disk, const ccos_inode_t* inode);

/**
 * @brief      Calculates the checksum of the content inode.
 *
 * @param[in]  disk           Compass disk image.
 * @param[in]  content_inode  The content inode.
 *
 * @return     The content inode checksum.
 */
uint16_t calc_content_inode_checksum(ccos_disk_t* disk, const ccos_content_inode_t* content_inode);

/**
 * @brief      Calculates the checksum of image's bitmask.
 *
 * @param[in]  disk     Compass disk image.
 * @param[in]  bitmask  CCOS image bitmask.
 *
 * @return     The bitmask checksum.
 */
uint16_t calc_bitmask_checksum(ccos_disk_t* disk, const ccos_bitmask_t* bitmask);

/**
 * @brief      Recalculate checksums of the inode.
 *
 * @param[in]  disk   Compass disk image.
 * @param      inode  The inode.
 */
void update_inode_checksums(ccos_disk_t* disk, ccos_inode_t* inode);

/**
 * @brief      Recalculate checksum of the content inode.
 *
 * @param[in]  disk           Compass disk image.
 * @param      content_inode  The content inode.
 */
void update_content_inode_checksums(ccos_disk_t* disk, ccos_content_inode_t* content_inode);

/**
 * @brief      Re-calculate and update CCOS image bitmask checksum.
 *
 * @param[in]  disk     Compass disk image.
 * @param      bitmask  CCOS image bitmask.
 */
void update_bitmask_checksum(ccos_disk_t* disk, ccos_bitmask_t* bitmask);

/**
 * @brief      Find a superblock (i.e. the inode with the root directory description) in a CCOS filesystem image.
 *
 * @param[in]  disk        Compass disk image.
 * @param      superblock  The superblock to return.
 *
 * @return     0 on success, with superblock numper passed out to the superblock parameter, -1 on error (i.e. in case of
 * invalid image format).
 */
int get_superblock(ccos_disk_t* disk, uint16_t* superblock);

/**
 * @brief      Get the CCOS filesystem inode at the given block.
 *
 * @param[in]  disk   Compass disk image.
 * @param[in]  block  The block number of the inode.
 *
 * @return     Pointer to CCOS filesystem inode structure.
 */
ccos_inode_t* get_inode(ccos_disk_t* disk, uint16_t block);

/**
 * @brief      Parse an inode and return the list of the file content blocks.
 *
 * @param[in]  disk          Compass disk image.
 * @param[in]  file          Inode first block number.
 * @param      blocks_count  The file content blocks count.
 * @param      blocks        The file content block numbers.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_file_blocks(ccos_disk_t* disk, ccos_inode_t* file, size_t* blocks_count, uint16_t** blocks);

/**
 * @brief      Get all bitmask blocks from the image.
 *
 * @param[in]  disk  Compass disk image.
 *
 * @return     List of CCOS image bitmask blocks.
 */
ccos_bitmask_list_t find_bitmask_blocks(ccos_disk_t* disk);

/**
 * @brief      Find available free block in the image and return it's number.
 *
 * @param[in]  disk          Compass disk image.
 * @param[in]  bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     The free block on success, CCOS_INVALID_BLOCK if no free space in the image.
 */
uint16_t get_free_block(ccos_disk_t* disk, const ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Mark block in the bitmask as free or used.
 *
 * @param[in]  disk          Compass disk image.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 * @param[in]  block         The number of the block.
 * @param[in]  mode          The mode (0 for free, 1 for used).
 */
void mark_block(ccos_disk_t* disk, ccos_bitmask_list_t* bitmask_list, uint16_t block, uint8_t mode);

/**
 * @brief      Initialize inode at the given block.
 *
 * @param[in]  disk              Compass disk image.
 * @param[in]  block             The block to create inode at.
 * @param[in]  parent_dir_block  The parent dir block.
 *
 * @return     Pointer to the newly created inode on success, NULL otherwise.
 */
ccos_inode_t* init_inode(ccos_disk_t* disk, uint16_t block, uint16_t parent_dir_block);

/**
 * @brief      Adds a new content inode (block with file block data) to the content inode list of the given file.
 *
 * @param[in]  disk          Compass disk image.
 * @param      file          The file to add content inode to.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     New content inode on success, NULL otherwise.
 */
ccos_content_inode_t* add_content_inode(ccos_disk_t* disk, ccos_inode_t* file, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Get content inode at the given block number.
 *
 * @param[in]  disk   Compass disk image.
 * @param[in]  block  Block number.
 *
 * @return     The content inode.
 */
ccos_content_inode_t* get_content_inode(ccos_disk_t* disk, uint16_t block);

/**
 * @brief      Gets the last content inode in the content inode list of the given file.
 *
 * @param[in]  disk  Compass disk image.
 * @param[in]  file  The file.
 *
 * @return     The last content inode on success, NULL otherwise.
 */
ccos_content_inode_t* get_last_content_inode(ccos_disk_t* disk, const ccos_inode_t* file);

/**
 * @brief      Cleanup image block at the given number and mark it as empty both in the image and in the image bitmask.
 *
 * @param[in]  disk          Compass disk image.
 * @param[in]  block         Block number.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 */
void erase_block(ccos_disk_t* disk, uint16_t block, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Removes the last content inode from the file's content inodes list, and erases this content inode block.
 *
 * @param[in]  disk          Compass disk image.
 * @param      file          The file.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_content_inode(ccos_disk_t* disk, ccos_inode_t* file, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Removes last content block from the file.
 *
 * @param[in]  disk          Compass disk image.
 * @param      file          The file.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     0 on success, -1 otherwise.
 */
int remove_block_from_file(ccos_disk_t* disk, ccos_inode_t* file, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Adds content block to the file.
 *
 * @param[in]  disk          Compass disk image.
 * @param      file          The file.
 * @param      bitmask_list  List of CCOS image bitmask blocks.
 *
 * @return     Block number of the added block on success, CCOS_INVALID_BLOCK otherwise.
 */
uint16_t add_block_to_file(ccos_disk_t* disk, ccos_inode_t* file, ccos_bitmask_list_t* bitmask_list);

/**
 * @brief      Add new file entry to the list of files in the given directory.
 *
 * @param[in]  disk       Compass disk image.
 * @param      directory  The directory to add file entry to.
 * @param      file       The file to add to the directory.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file_to_directory(ccos_disk_t* disk, ccos_inode_t* directory, ccos_inode_t* file);

/**
 * @brief      Insert new directory entry into the directory, effectively making this directory a parent for the given
 * file.
 *
 * @param[in]  disk       Compass disk image.
 * @param      directory  The directory.
 * @param      file       The file.
 *
 * @return     0 on success, -1 otherwise.
 */
int add_file_entry_to_dir_contents(ccos_disk_t* disk, ccos_inode_t* directory, ccos_inode_t* file);

/**
 * @brief      Delete file entry from the parent directory.
 *
 * @param[in]  disk  Compass disk image.
 * @param      file  The file.
 *
 * @return     0 on success, -1 otherwise.
 */
int delete_file_from_parent_dir(ccos_disk_t* disk, ccos_inode_t* file);

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
 * @param[in]  disk                 Compass disk image.
 * @param[in]  directory_data       Directory raw contents.
 * @param[in]  directory_data_size  Directory contents size.
 * @param[in]  entry_count          Number of files in the directory.
 * @param      entries              Array of files located in the directory.
 *
 * @return     0 on success, -1 otherwise.
 */
int parse_directory_data(ccos_disk_t* disk,
                         const uint8_t* directory_data, size_t directory_data_size,
                         uint16_t entry_count, parsed_directory_element_t** entries);

/**
 * @brief      Read raw data from the image at a given block. Notice it won't allocate any memory, just return a pointer
 * and a size of a raw data inside a block.
 *
 * @param[in]  disk   Compass disk image.
 * @param[in]  block  Block number.
 * @param      start  Start address of the raw data.
 * @param      size   The size of a raw data.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_block_data(ccos_disk_t* disk, uint16_t block, const uint8_t** start, size_t* size);

/**
 * @brief      Return info about free blocks in a CCOS image.
 *
 * @param[in]  disk               Compass disk image.
 * @param[in]  bitmask_list       List of CCOS image bitmask blocks.
 * @param      free_blocks_count  Pointer to free blocks count.
 * @param      free_blocks        Pointer to the free blocks array.
 *
 * @return     0 on success, -1 otherwise.
 */
int get_free_blocks(ccos_disk_t* disk, ccos_bitmask_list_t* bitmask_list,
                    size_t* free_blocks_count, uint16_t** free_blocks) ;

/**
 * @brief      Find the index of a file in the directory data.
 *
 * @param[in]  file      The file to find.
 * @param[in]  directory The directory to search in.
 * @param[in]  elements  Array of parsed directory elements.
 *
 * @return     Index of the file in the directory on success, -1 otherwise.
 */
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
 * @brief      Changes the date of a file or folder.
 *
 * @param[in]  disk      Compass disk image.
 * @param      file      The file or the directory.
 * @param      new_date  The new date variable.
 * @param      type      Date type to replace (CREATED - creation, MODIF - modification, EXPIR - expiration).
 *
 * @return     0 on success, -1 otherwise.
 */
int change_date(ccos_disk_t* disk, ccos_inode_t* file, ccos_date_t new_date, date_type_t type);

#endif  // CCOS_DISK_TOOL_CCOS_PRIVATE_H
