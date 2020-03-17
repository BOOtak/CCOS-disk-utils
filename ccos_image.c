#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ccos_image.h>
#include <common.h>

#define FAT_MBR_END_OF_SECTOR_MARKER 0xAA55
#define OPCODE_NOP 0x90
#define OPCODE_JMP 0xEB

/**
 * Superblock is a block which contains root directory description. Usually CCOS disc image contains superblock number
 * at the offset 0x20. Sometimes though, it doesn't. In such cases we assume that superblock is # 0x121.
 */
#define CCOS_SUPERBLOCK_ADDR_OFFSET 0x20
#define CCOS_DEFAULT_SUPERBLOCK 0x121

/**
 * Those are typical superblock values observed from the real CCOS floppy images. 0x121 is usual for 360kb images, 0x6
 * - for the 720kb ones.
 */
#define TYPICAL_SUPERBLOCK_1 0x121
#define TYPICAL_SUPERBLOCK_2 0x06

#define CCOS_DIR_NAME_OFFSET 0x8

#define CCOS_FILE_BLOCKS_OFFSET 0xD2
#define CCOS_BLOCK_1_OFFSET 0xD8
#define CCOS_BLOCK_N_OFFSET 0x0C

#define CCOS_CONTENT_BLOCKS_END_MARKER 0xFFFF
#define CCOS_BLOCK_END_MARKER 0x0000

#define CCOS_DATA_OFFSET 0x4
#define CCOS_BLOCK_SUFFIX_LENGTH 0x4

#define CCOS_DIR_ENTRIES_OFFSET 0x1
#define CCOS_DIR_ENTRY_SUFFIX_LENGTH 0x2
#define CCOS_DIR_LAST_ENTRY_MARKER 0xFF00
#define CCOS_DIR_TYPE "subject"

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

static int is_fat_image(const uint8_t* data) {
  return ((data[0] == OPCODE_JMP) && (data[2] == OPCODE_NOP) &&
          (*(uint16_t*)&(data[0x1FE]) == FAT_MBR_END_OF_SECTOR_MARKER));
}

static int is_imd_image(const uint8_t* data) {
  return data[0] == 'I' && data[1] == 'M' && data[2] == 'D' && data[3] == ' ';
}

int ccos_get_superblock(const uint8_t* data, size_t image_size, uint16_t* superblock) {
  if (is_fat_image(data)) {
    fprintf(stderr, "FAT floppy image is found; return.\n");
    return -1;
  }

  if (is_imd_image(data)) {
    fprintf(stderr,
            "Provided image is in ImageDisk format, please convert it into the raw disk\n"
            "image (.img) before using.\n"
            "\n"
            "(You can use Disk-Utilities from here: https://github.com/keirf/Disk-Utilities)\n");
    return -1;
  }

  uint16_t res = *((uint16_t*)&(data[CCOS_SUPERBLOCK_ADDR_OFFSET]));
  if (res == 0) {
    res = CCOS_DEFAULT_SUPERBLOCK;
  }

  if (res != TYPICAL_SUPERBLOCK_1 && res != TYPICAL_SUPERBLOCK_2) {
    fprintf(stderr, "Warn: Unusual superblock value 0x%x\n", res);
  }

  uint32_t blocks_in_image = image_size / BLOCK_SIZE;
  if (res > blocks_in_image) {
    fprintf(stderr, "Invalid superblock! (Superblock: 0x%x, but only 0x%x blocks in the image).\n", res,
            blocks_in_image);
    return -1;
  }

  uint32_t addr = res * BLOCK_SIZE;
  uint16_t block_header = *(uint16_t*)&(data[addr]);
  if (block_header != res) {
    fprintf(stderr, "Invalid image: Block header 0x%x mismatches superblock 0x%x!\n", block_header, res);
    return -1;
  }

  *superblock = res;
  return 0;
}

const ccos_inode_t* ccos_get_inode(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  return (const ccos_inode_t*)&(data[addr]);
}

version_t ccos_get_file_version(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  uint8_t major = inode->version_major;
  uint8_t minor = inode->version_minor;
  uint8_t patch = inode->version_patch;
  version_t version = {major, minor, patch};
  return version;
}

const short_string_t* ccos_get_file_name(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  return (const short_string_t*)&(inode->name_length);
}

uint32_t ccos_get_file_size(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  return inode->file_size;
}

ccos_date_t ccos_get_creation_date(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  return inode->creation_date;
}

ccos_date_t ccos_get_mod_date(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  return inode->mod_date;
}

ccos_date_t ccos_get_exp_date(uint16_t block, const uint8_t* data) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  return inode->expiration_date;
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
 * @param[in]  data          The CCOS image data.
 * @param[in]  offset        The CCOS image offset of the content blocks.
 * @param      blocks_count  The blocks count.
 * @param      blocks        The block numbers.
 *
 * @return     One of three values:
 * - CONTENT_END_MARKER - special CCOS_CONTENT_BLOCKS_END_MARKER value was encountered during the read. It means that
 * this inode block was the last one.
 * - BLOCK_END_MARKER - special CCOS_BLOCK_END_MARKER value was encountered. This value indicates the end of the current
 * block. It means that another inode block contains subsequent file content blocks.
 * - END_OF_BLOCK - this status usually indicates that we reached out of the current block bounds without encountering
 * CCOS_CONTENT_BLOCKS_END_MARKER or CCOS_BLOCK_END_MARKER, which, in general, should be considered as an error.
 * However, some images may actually be like this and still work fine, usually when data blocks end near block end, take
 * this GRIDOS31 image from rou021:
 *
 * d752d11890432c66b4201427096c7412f72cc2a4  GRIDOS31.IMD
 * e1e29435ba51b7bcc1281a928eac65970a959d3b  GRIDOS31.IMG
 *
 * 000849E0:  AA 04 AB 04-AC 04 AD 04-AE 04 AF 04-B0 04 B1 04
 * 000849F0:  B2 04 B3 04-B4 04 B5 04-B6 04 B7 04-03 14 54 79
 *                                                ^
 *                                                end of block, should be all zeroes
 *
 * So, as this happens, and system works fine in such conditions, we should not treat this as an error.
 */
static read_block_status_t read_blocks(const uint8_t* data, uint32_t offset, size_t* blocks_count, uint16_t* blocks) {
  int i = 0;
  uint32_t start_addr = (offset / BLOCK_SIZE) * BLOCK_SIZE;
  uint32_t end_addr = start_addr + BLOCK_SIZE;
  uint32_t end_of_block_addr = end_addr - 4;
  while (offset % BLOCK_SIZE != 0) {
    uint16_t block = *((uint16_t*)&(data[offset]));
    if (block == CCOS_CONTENT_BLOCKS_END_MARKER) {
      return CONTENT_END_MARKER;
    } else if (block == CCOS_BLOCK_END_MARKER) {
      return BLOCK_END_MARKER;
    }

    if (offset >= end_of_block_addr) {
      return END_OF_BLOCK;
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
    fprintf(stderr, "Warn: Unexpected END_OF_BLOCK encountered while reading file block list at block 0x%x!\n", block);
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
        read_blocks(data, content2_addr, &content2_blocks_count, &((*blocks)[real_blocks_count]));
    if (status == END_OF_BLOCK) {
      fprintf(stderr, "Warn: Unexpected END_OF_BLOCK encountered while reading file block list at block 0x%x!\n",
              block);
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

int ccos_get_dir_contents(uint16_t block, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_inodes) {
  uint16_t* dir_blocks = NULL;
  size_t blocks_count = 0;

  if (ccos_get_file_blocks(block, data, &blocks_count, &dir_blocks) == -1) {
    return -1;
  }

  uint32_t dir_size = ccos_get_file_size(block, data);

  uint8_t* dir_contents = (uint8_t*)calloc(dir_size, sizeof(uint8_t));
  if (dir_contents == NULL) {
    return -1;
  }

  size_t offset = 0;
  for (size_t i = 0; i < blocks_count; ++i) {
    const uint8_t* start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(dir_blocks[i], data, &start, &data_size) == -1) {
      free(dir_blocks);
      free(dir_contents);
      return -1;
    }

    memcpy(dir_contents + offset, start, MIN(data_size, dir_size - offset));
    offset += data_size;
  }

  int res = parse_directory_contents(dir_contents, dir_size, entry_count, entries_inodes);
  free(dir_contents);
  free(dir_blocks);
  return res;
}

int ccos_is_dir(uint16_t block, const uint8_t* data) {
  char type[CCOS_MAX_FILE_NAME];
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(ccos_get_file_name(block, data), NULL, type);
  if (res == -1) {
    return 0;
  }

  return strncasecmp(type, CCOS_DIR_TYPE, strlen(CCOS_DIR_TYPE)) == 0;
}

int ccos_parse_file_name(const short_string_t* file_name, char* basename, char* type) {
  char* delim = strchr(file_name->data, '~');
  if (delim == NULL) {
    fprintf(stderr, "Invalid name \"%.*s\": no file type found!\n", file_name->length, file_name->data);
    return -1;
  }

  char* last_char = strchr(delim + 1, '~');
  if ((last_char + 1 - file_name->data) != file_name->length) {
    fprintf(stderr, "Invalid name \"%.*s\": invalid file type format!\n", file_name->length, file_name->data);
    return -1;
  }

  if (basename != NULL) {
    strncpy(basename, file_name->data, (delim - file_name->data));
  }

  if (type != NULL) {
    strncpy(type, delim + 1, strlen(delim + 1) - 1);
  }

  return 0;
}

int ccos_replace_file(uint16_t block, const uint8_t* file_data, uint32_t file_size, uint8_t* image_data) {
  uint32_t inode_file_size = ccos_get_file_size(block, image_data);
  if (inode_file_size != file_size) {
    fprintf(stderr,
            "Unable to write file: File size mismatch!\n"
            "(size from the block: %d bytes; actual size: %d bytes\n",
            inode_file_size, file_size);
    return -1;
  }

  size_t block_count = 0;
  uint16_t* blocks = NULL;
  if (ccos_get_file_blocks(block, image_data, &block_count, &blocks) != 0) {
    fprintf(stderr, "Unable to write file to image: Unable to get file blocks from the block!\n");
    return -1;
  }

  const uint8_t* image_data_part = file_data;
  size_t written_size = 0;
  for (size_t i = 0; i < block_count; ++i) {
    const uint8_t* start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(blocks[i], image_data, &start, &data_size) != 0) {
      fprintf(stderr, "Unable to write data: Unable to get target block address!\n");
      free(blocks);
      return -1;
    }

    memcpy((uint8_t*)start, image_data_part, MIN(data_size, file_size - written_size));
    image_data_part += data_size;
    written_size += data_size;
  }

  free(blocks);
  return 0;
}
