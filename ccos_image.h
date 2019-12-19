#ifndef CCOS_IMAGE_H
#define CCOS_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 512

#define CCOS_MAX_FILE_NAME 80

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} version_t;

struct short_string_t_;
typedef struct short_string_t_ short_string_t;

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
 * @brief      Get the file version.
 *
 * @param[in]  inode  The file inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The version of the file.
 */
version_t ccos_get_file_version(uint16_t inode, const uint8_t* data);

/**
 * @brief      Get the name of the file.
 *
 * @param[in]  inode  The file inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     Pointer to short string with a file name.
 */
const short_string_t* ccos_get_file_name(uint16_t inode, const uint8_t* data);

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
 * @param[in]  inode           The directory inode.
 * @param[in]  data            CCOS image data.
 * @param      entry_count     Count of the items in the directory.
 * @param      entries_blocks  Directory contents inodes.
 *
 * @return     0 on success, -1 otherwise.
 */
int ccos_get_dir_contents(uint16_t inode, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_inodes);

/**
 * @brief      Determine whether the given inode is a directory's inode.
 *
 * @param[in]  inode  The inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     1 if the given inode belongs to a directory, 0 otherwise.
 */
int ccos_is_dir(uint16_t inode, const uint8_t* data);

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
int ccos_get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size);

/**
 * @brief      Get the size of a file at a given inode.
 *
 * @param[in]  inode  The inode.
 * @param[in]  data   CCOS image data.
 *
 * @return     The size of a file at a given inode.
 */
uint32_t ccos_get_file_size(uint16_t inode, const uint8_t* data);

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

#endif  // CCOS_IMAGE_H
