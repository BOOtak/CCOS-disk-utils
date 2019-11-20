#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 512

/**
 * Those are typical superblock values observed from the real CCOS floppy images. 0x121 is usual for 360kb images, 0x6E
 * - for the 720kb ones.
 */
#define TYPICAL_SUPERBLOCK_1 0x121
#define TYPICAL_SUPERBLOCK_2 0x6E

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} version_t;

struct short_string_t_;
typedef struct short_string_t_ short_string_t;

typedef void (*on_block_read_callback_t)(const uint8_t* start, size_t offset);

uint16_t ccos_get_superblock(const uint8_t* data);

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
