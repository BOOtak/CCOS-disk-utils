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

typedef void (*on_block_read_callback_t)(const uint8_t* start, size_t offset);

int ccos_get_superblock(const uint8_t* data, size_t image_size, uint16_t* superblock);

version_t ccos_get_file_version(uint16_t block, const uint8_t* data);

const short_string_t* ccos_get_file_name(uint16_t inode, const uint8_t* data);

char* ccos_short_string_to_string(const short_string_t* short_string);

/**
 * @brief      Parse an inode and return the list of the file content blocks
 *
 * @param[in]  block         Inode first block number
 * @param[in]  data          CCOS image data
 * @param      blocks_count  The file content blocks count
 * @param      blocks        The file content block numbers
 *
 * @return     0 on success, -1 otherwise
 */
int ccos_get_file_blocks(uint16_t block, const uint8_t* data, size_t* blocks_count, uint16_t** blocks);

int ccos_get_dir_contents(uint16_t inode, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_blocks);

int ccos_is_dir(uint16_t inode, const uint8_t* data);

int ccos_get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size);

uint32_t ccos_get_file_size(uint16_t inode, const uint8_t* data);

/**
 * @brief      Perse CCOS file name and return it's basename and it's type
 *
 * @param[in]  file_name  The file name
 * @param      basename   The basename
 * @param      type       The file type
 *
 * @return     0 on success, -1 otherwise
 */
int ccos_parse_file_name(const short_string_t* file_name, char* basename, char* type);
