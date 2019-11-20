#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ccos_image.h>

/**
 * Superblock is a block which contains root directory description. Usually CCOS disc image contains superblock number
 * at the offset 0x20. Sometimes though, it doesn't. In such cases we assume that superblock is # 0x121.
 */
#define CCOS_SUPERBLOCK_ADDR_OFFSET 0x20
#define CCOS_DEFAULT_SUPERBLOCK 0x121

#define CCOS_VERSION_MAJOR_OFFSET 0x96
#define CCOS_VERSION_MINOR_OFFSET 0x97
#define CCOS_VERSION_PATCH_OFFSET 0xA7

#define CCOS_DIR_NAME_OFFSET 0x8

#define CCOS_FILE_BLOCKS_OFFSET 0xD2
#define CCOS_BLOCK_1_OFFSET 0xD8
#define CCOS_BLOCK_N_OFFSET 0x12

#define CCOS_CONTENT_BLOCKS_END_MARKER 0xFFFF
#define CCOS_BLOCK_END_MARKER 0x0000

#define CCOS_DATA_OFFSET 0x4
#define CCOS_BLOCK_SUFFIX_LENGTH 0x4

#define CCOS_DIR_ENTRIES_OFFSET 0x1
#define CCOS_DIR_ENTRY_SUFFIX_LENGTH 0x2
#define CCOS_DIR_LAST_ENTRY_MARKER 0xFF00
#define CCOS_DIR_TYPE "subject"

#define CCOS_INODE_FILE_SIZE_OFFSET 0x4

#define MIN(A, B) A < B ? A : B

#define TR fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__)

struct short_string_t_ {
  uint8_t length;
  const char data[0xFF];
};

#pragma pack(push, 1)
typedef struct {
  uint16_t block;
  uint8_t name_length;
} dir_entry_t;
#pragma pack(pop)

typedef enum { CONTENT_END_MARKER, BLOCK_END_MARKER, END_OF_BLOCK } read_block_status_t;

uint16_t ccos_get_superblock(const uint8_t* data) {
  uint16_t superblock = *((uint16_t*)&(data[CCOS_SUPERBLOCK_ADDR_OFFSET]));
  if (superblock == 0) {
    return CCOS_DEFAULT_SUPERBLOCK;
  } else {
    return superblock;
  }
}

version_t ccos_get_file_version(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  uint8_t major = data[addr + CCOS_VERSION_MAJOR_OFFSET];
  uint8_t minor = data[addr + CCOS_VERSION_MINOR_OFFSET];
  uint8_t patch = data[addr + CCOS_VERSION_PATCH_OFFSET];
  version_t version = {major, minor, patch};
  return version;
}

const short_string_t* ccos_get_file_name(uint16_t inode, const uint8_t* data) {
  uint32_t addr = inode * BLOCK_SIZE;
  return (const short_string_t*)&(data[addr + CCOS_DIR_NAME_OFFSET]);
}

uint32_t ccos_get_file_size(uint16_t inode, const uint8_t* data) {
  size_t addr = inode * BLOCK_SIZE;
  return *(uint32_t*)&(data[addr + CCOS_INODE_FILE_SIZE_OFFSET]);
}

char* ccos_short_string_to_string(const short_string_t* short_string) {
  char* result = calloc(short_string->length + 1, sizeof(char));
  if (result == NULL) {
    return NULL;
  }

  memcpy(result, short_string->data, short_string->length);
  return result;
}

/**
 * @brief      Reads file content block numbers from the inode block.
 *
 * @param[in]  data          The CCOS image data
 * @param[in]  offset        The CCOS image offset of the content blocks
 * @param      blocks_count  The blocks count
 * @param      blocks        The block numbers
 *
 * @return     One of three values:
 * - CONTENT_END_MARKER - special CCOS_CONTENT_BLOCKS_END_MARKER value was encountered during the read. It means that
 * this inode block was the last one.
 * - BLOCK_END_MARKER - special CCOS_BLOCK_END_MARKER value was encountered. This value indicates the end of the current
 * block. It means that another inode block contains subsequent file content blocks.
 * - END_OF_BLOCK - this status usually indicates that we reached out of the current block bounds without encountering
 * CCOS_CONTENT_BLOCKS_END_MARKER or CCOS_BLOCK_END_MARKER, which, in general, is an error.
 */
static read_block_status_t read_blocks(const uint8_t* data, uint32_t offset, size_t* blocks_count, uint16_t* blocks) {
  int i = 0;
  while (offset % BLOCK_SIZE != 0) {
    uint16_t block = *((uint16_t*)&(data[offset]));
    if (block == CCOS_CONTENT_BLOCKS_END_MARKER) {
      return CONTENT_END_MARKER;
    } else if (block == CCOS_BLOCK_END_MARKER) {
      return BLOCK_END_MARKER;
    }

    offset += sizeof(uint16_t);
    *blocks_count = (*blocks_count) + 1;
    blocks[i++] = block;
  }

  return END_OF_BLOCK;
}

int ccos_get_file_blocks(uint16_t block, const uint8_t* data, size_t* blocks_count, uint16_t** blocks) {
  uint32_t addr = block * BLOCK_SIZE;
  uint16_t block1 = *((uint16_t*)&(data[addr + CCOS_FILE_BLOCKS_OFFSET + 2]));
  uint16_t block2 = *((uint16_t*)&(data[addr + CCOS_FILE_BLOCKS_OFFSET]));

  uint32_t content1_addr = block1 * BLOCK_SIZE + CCOS_BLOCK_1_OFFSET;
  size_t max_blocks_count = BLOCK_SIZE - CCOS_BLOCK_1_OFFSET;
  *blocks = calloc(max_blocks_count, sizeof(uint16_t));
  if (*blocks == NULL) {
    return -1;
  }

  size_t real_blocks_count = 0;
  if (read_blocks(data, content1_addr, &real_blocks_count, *blocks) == END_OF_BLOCK) {
    free(*blocks);
    return -1;
  }

  *blocks_count = real_blocks_count;

  // TODO: I've never encountered CCOS disc image with file larger than 170 kb. So, I've never seen inode larger than
  // two blocks. The code below handles two-block inode, but this has to be modified to read larger files. I don't know
  // how subsequent inode blocks reference each other, probably through some double-linked list, where each inode block
  // references previous and next blocks. However, this needs to be proven, until then, we're just print error message
  // and crash, if we encounter an inode which is obviously larger than two blocks (e.g, we met end-of-block marker
  // before end-of-blocks-descrtiption marker in the second inode block).
  if (block2 != 0xFFFF) {
    uint32_t content2_addr = block2 * BLOCK_SIZE + CCOS_BLOCK_N_OFFSET;
    max_blocks_count = max_blocks_count + BLOCK_SIZE - CCOS_BLOCK_1_OFFSET;
    uint16_t* new_blocks = realloc(*blocks, sizeof(uint16_t) * max_blocks_count);
    if (new_blocks == NULL) {
      free(*blocks);
      return -1;
    } else {
      *blocks = new_blocks;
    }

    size_t content2_blocks_count = 0;
    read_block_status_t status =
        read_blocks(data, content2_addr, &content2_blocks_count, &(*blocks[real_blocks_count]));
    if (status == END_OF_BLOCK) {
      fprintf(stderr, "End of block encountered in read_blocks!\n");
      free(*blocks);
      return -1;
    } else if (status == BLOCK_END_MARKER) {
      fprintf(stderr,
              "BLOCK_END_MARKER encountered in read_blocks of the second inode "
              "block!\n");
      free(*blocks);
      return -1;
    }

    *blocks_count += content2_blocks_count;
  }

  return 0;
}

int ccos_get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size) {
  // TODO: check bounds
  uint32_t address = block * BLOCK_SIZE;
  *start = &(data[address + CCOS_DATA_OFFSET]);
  *size = BLOCK_SIZE - (CCOS_DATA_OFFSET + CCOS_BLOCK_SUFFIX_LENGTH);
  return 0;
}

static int parse_directory_contents(const uint8_t* data, size_t data_size, uint16_t* entry_count,
                                    uint16_t** entries_blocks) {
  // worst-case scenario: all the entries have empty names
  size_t max_entry_count = data_size / (CCOS_DIR_ENTRY_SUFFIX_LENGTH + sizeof(dir_entry_t));
  *entries_blocks = (uint16_t*)calloc(max_entry_count, sizeof(uint16_t));
  if (*entries_blocks == NULL) {
    return -1;
  }

  uint16_t count = 0;
  size_t offset = CCOS_DIR_ENTRIES_OFFSET;
  while (offset < data_size) {
    dir_entry_t* entry = (dir_entry_t*)&(data[offset]);

    (*entries_blocks)[count++] = entry->block;
    uint16_t file_suffix = *(uint16_t*)&(data[offset + sizeof(dir_entry_t) + entry->name_length]);

    if ((file_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
      break;
    }

    offset += sizeof(dir_entry_t) + entry->name_length + CCOS_DIR_ENTRY_SUFFIX_LENGTH;
  }

  *entry_count = count;
  return 0;
}

int ccos_get_dir_contents(uint16_t inode, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_blocks) {
  uint16_t* dir_blocks = NULL;
  size_t blocks_count = 0;

  if (ccos_get_file_blocks(inode, data, &blocks_count, &dir_blocks) == -1) {
    return -1;
  }

  uint32_t dir_size = ccos_get_file_size(inode, data);

  uint8_t* dir_contents = (uint8_t*)calloc(dir_size, sizeof(uint8_t));
  if (dir_contents == NULL) {
    return -1;
  }

  size_t offset = 0;
  for (size_t i = 0; i < blocks_count; ++i) {
    const uint8_t* start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(dir_blocks[i], data, &start, &data_size) == -1) {
      free(dir_contents);
      return -1;
    }

    memcpy(dir_contents + offset, start, MIN(data_size, dir_size - offset));
    offset += data_size;
  }

  int res = parse_directory_contents(dir_contents, dir_size, entry_count, entries_blocks);
  free(dir_contents);
  return res;
}

int ccos_is_dir(uint16_t inode, const uint8_t* data) {
  char* filename = ccos_short_string_to_string(ccos_get_file_name(inode, data));
  if (filename == NULL) {
    return 0;
  }

  char* type_index = strchr(filename, '~');
  if (type_index == NULL) {
    free(filename);
    return 0;
  }

  int res = strncasecmp(type_index + 1, CCOS_DIR_TYPE, strlen(CCOS_DIR_TYPE)) == 0;
  free(filename);
  return res;
}
