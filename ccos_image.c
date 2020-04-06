#include <ccos_image.h>
#include <common.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define CCOS_INVALID_BLOCK 0xFFFF

#define CCOS_DATA_OFFSET 0x4
#define CCOS_BLOCK_SUFFIX_LENGTH 0x4
#define CCOS_BLOCK_DATA_SIZE (BLOCK_SIZE - (CCOS_DATA_OFFSET + CCOS_BLOCK_SUFFIX_LENGTH))

#define CCOS_DIR_ENTRIES_OFFSET 0x1
#define CCOS_DIR_ENTRY_SUFFIX_LENGTH 0x2
#define CCOS_DIR_LAST_ENTRY_MARKER 0xFF00
#define CCOS_DIR_TYPE "subject"

#define CCOS_EMPTY_BLOCK_MARKER 0xFFFFFFFF

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

uint16_t ccos_make_checksum(const uint8_t* data, uint16_t data_size) {
  uint16_t ret = 0;
  for (int i = 0; i < data_size; i += 2) {
    if (i + 2 > data_size) {
      ret += data[0];
    } else {
      ret += (data[1] << 8) | (data[0]);
    }
    data += 2;
  }

  return ret;
}

uint16_t ccos_make_inode_metadata_checksum(const ccos_inode_t* inode) {
  return ccos_make_checksum((const uint8_t*)&(inode->header), offsetof(ccos_inode_t, metadata_checksum));
}

uint16_t ccos_make_inode_blocks_checksum(const ccos_inode_t* inode) {
  uint16_t blocks_checksum =
      ccos_make_checksum((const uint8_t*)&(inode->content_inode_info.block_next),
                         offsetof(ccos_inode_t, block_end) - offsetof(ccos_inode_t, content_inode_info) -
                             offsetof(ccos_block_data_t, block_next));
  blocks_checksum += inode->content_inode_info.header.file_id;
  blocks_checksum += inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
}

uint16_t ccos_make_content_inode_checksum(const ccos_content_inode_t* content_inode) {
  uint16_t blocks_checksum = ccos_make_checksum((const uint8_t*)&(content_inode->content_inode_info.block_next),
                                                offsetof(ccos_content_inode_t, block_end) -
                                                    offsetof(ccos_content_inode_t, content_inode_info) -
                                                    offsetof(ccos_block_data_t, block_next));
  blocks_checksum += content_inode->content_inode_info.header.file_id;
  blocks_checksum += content_inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
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

ccos_inode_t* ccos_get_inode(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  return (ccos_inode_t*)&(data[addr]);
}

static ccos_content_inode_t* ccos_get_content_inode(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  return (ccos_content_inode_t*)&(data[addr]);
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

int ccos_get_file_blocks(uint16_t block, const uint8_t* data, size_t* blocks_count, uint16_t** blocks) {
  const ccos_inode_t* inode = ccos_get_inode(block, data);
  *blocks = calloc(MAX_BLOCKS_IN_INODE, sizeof(uint16_t));
  if (*blocks == NULL) {
    return -1;
  }

  size_t real_blocks_count = 0;
  for (int i = 0; i < MAX_BLOCKS_IN_INODE; ++i) {
    uint16_t content_block = inode->content_blocks[i];
    if (content_block == CCOS_CONTENT_BLOCKS_END_MARKER) {
      break;
    }

    (*blocks)[real_blocks_count++] = content_block;
  }

  TRACE("Block count in 0x%lx itself: %d", block, real_blocks_count);

  if (inode->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has more than 1 block!");
    const ccos_content_inode_t* content_inode = ccos_get_content_inode(inode->content_inode_info.block_next, data);
    for (;;) {
      TRACE("Processing extra block 0x%lx...", inode->content_inode_info.block_next);

      uint16_t checksum = ccos_make_content_inode_checksum(content_inode);

      if (checksum != content_inode->content_inode_info.blocks_checksum) {
        fprintf(stderr, "Warn: Blocks checksum mismatch: expected 0x%04hx, got 0x%04hx\n",
                content_inode->content_inode_info.blocks_checksum, checksum);
      }

      uint16_t* extra_blocks = calloc(MAX_BLOCKS_IN_CONTENT_INODE, sizeof(uint16_t));
      if (extra_blocks == NULL) {
        fprintf(stderr, "Unable to allocate memory for extra blocks: %s!\n", strerror(errno));
        free(*blocks);
        return -1;
      }

      size_t extra_blocks_count = 0;
      for (int i = 0; i < MAX_BLOCKS_IN_CONTENT_INODE; ++i) {
        uint16_t content_block = content_inode->content_blocks[i];
        if (content_block == CCOS_CONTENT_BLOCKS_END_MARKER) {
          break;
        }

        extra_blocks[extra_blocks_count++] = content_block;
      }

      TRACE("Extra block has %d blocks", extra_blocks_count);

      uint16_t* new_blocks = realloc(*blocks, sizeof(uint16_t) * (real_blocks_count + extra_blocks_count));
      if (new_blocks == NULL) {
        fprintf(stderr, "Unable to realloc memory for content blocks: %s!\n", strerror(errno));
        free(*blocks);
        return -1;
      } else {
        *blocks = new_blocks;
      }

      memcpy(&(*blocks)[real_blocks_count], extra_blocks, sizeof(uint16_t) * extra_blocks_count);
      real_blocks_count += extra_blocks_count;

      if (content_inode->content_inode_info.block_next == CCOS_INVALID_BLOCK) {
        break;
      }

      content_inode = ccos_get_content_inode(content_inode->content_inode_info.block_next, data);
    }
  }

  *blocks_count = real_blocks_count;
  return 0;
}

int ccos_get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size) {
  // TODO: check bounds
  uint32_t address = block * BLOCK_SIZE;
  *start = &(data[address + CCOS_DATA_OFFSET]);
  *size = CCOS_BLOCK_DATA_SIZE;
  return 0;
}

static int parse_directory_contents(const uint8_t* data, size_t data_size, uint16_t entry_count,
                                    uint16_t** entries_blocks) {
  *entries_blocks = (uint16_t*)calloc(entry_count, sizeof(uint16_t));
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

  return 0;
}

int ccos_get_dir_contents(uint16_t block, const uint8_t* data, uint16_t* entry_count, uint16_t** entries_inodes) {
  uint16_t* dir_blocks = NULL;
  size_t blocks_count = 0;

  if (ccos_get_file_blocks(block, data, &blocks_count, &dir_blocks) == -1) {
    return -1;
  }

  const ccos_inode_t* inode = ccos_get_inode(block, data);
  uint32_t dir_size = inode->file_size;
  *entry_count = inode->dir_count;

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

  int res = parse_directory_contents(dir_contents, dir_size, *entry_count, entries_inodes);
  free(dir_contents);
  free(dir_blocks);
  return res;
}

int ccos_is_dir(uint16_t block, const uint8_t* data) {
  char type[CCOS_MAX_FILE_NAME];
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(ccos_get_file_name(block, data), NULL, type, NULL, NULL);
  if (res == -1) {
    return 0;
  }

  return strncasecmp(type, CCOS_DIR_TYPE, strlen(CCOS_DIR_TYPE)) == 0;
}

int ccos_parse_inode_name(ccos_inode_t* inode, char* basename, char* type, size_t* name_length, size_t* type_length) {
  return ccos_parse_file_name((const short_string_t*)&(inode->name_length), basename, type, name_length, type_length);
}

int ccos_parse_file_name(const short_string_t* file_name, char* basename, char* type, size_t* name_length,
                         size_t* type_length) {
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

  if (name_length != NULL) {
    *name_length = (delim - file_name->data);
  }

  if (type_length != NULL) {
    *type_length = strlen(delim + 1) - 1;
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

int ccos_get_free_blocks(const uint8_t* data, size_t data_size, uint16_t** free_blocks, size_t* free_blocks_count) {
  size_t block_count = data_size / BLOCK_SIZE;
  *free_blocks_count = 0;
  for (int i = 0; i < block_count; ++i) {
    uint32_t block_header = *(uint32_t*)&(data[i * BLOCK_SIZE]);
    if (block_header == CCOS_EMPTY_BLOCK_MARKER) {
      *free_blocks_count = *free_blocks_count + 1;
    }
  }

  *free_blocks = (uint16_t*)calloc(*free_blocks_count, sizeof(uint16_t));
  if (*free_blocks == NULL) {
    fprintf(stderr, "Unable to allocate %ld bytes for free blocks: %s!\n", *free_blocks_count * sizeof(uint16_t),
            strerror(errno));
    return -1;
  }

  size_t out_free_blocks_count = 0;
  for (int i = 0; i < block_count; ++i) {
    uint32_t block_header = *(uint32_t*)&(data[i * BLOCK_SIZE]);
    if (block_header == CCOS_EMPTY_BLOCK_MARKER) {
      (*free_blocks)[out_free_blocks_count++] = i;
    }
  }

  return 0;
}

int ccos_get_image_map(const uint8_t* data, size_t data_size, block_type_t** image_map, size_t* free_blocks_count) {
  size_t block_count = data_size / BLOCK_SIZE;
  if (block_count * BLOCK_SIZE != data_size) {
    fprintf(stderr, "Warn: image size (%lu bytes) is not a multiple of a block size (%d bytes)\n", data_size,
            BLOCK_SIZE);
  }

  *image_map = (block_type_t*)calloc(block_count, sizeof(block_type_t));
  if (*image_map == NULL) {
    fprintf(stderr, "Unable to allocate memory for %ld blocks in block map: %s!\n", block_count, strerror(errno));
    return -1;
  }

  *free_blocks_count = 0;
  for (int i = 0; i < block_count; ++i) {
    uint32_t block_header = *(uint32_t*)&(data[i * BLOCK_SIZE]);
    block_type_t block_type = UNKNOWN;
    if (block_header == CCOS_EMPTY_BLOCK_MARKER) {
      *free_blocks_count = *free_blocks_count + 1;
      block_type = EMPTY;
    } else {
      block_type = DATA;
    }

    (*image_map)[i] = block_type;
  }

  return 0;
}

int ccos_update_checksums(ccos_inode_t* inode) {
  inode->metadata_checksum = ccos_make_inode_metadata_checksum(inode);
  inode->content_inode_info.blocks_checksum = ccos_make_inode_blocks_checksum(inode);
  return 0;
}

int ccos_update_content_inode_checksums(ccos_content_inode_t* content_inode) {
  content_inode->content_inode_info.blocks_checksum = ccos_make_content_inode_checksum(content_inode);
  return 0;
}

ccos_inode_t* ccos_create_inode(uint16_t block, uint16_t parent_dir_block, uint8_t* image_data) {
  TRACE("Creating new inode at 0x%x!", block);
  ccos_inode_t* inode = ccos_get_inode(block, image_data);
  memset(inode, 0, sizeof(ccos_inode_t));
  inode->header.file_id = block;
  inode->content_inode_info.header.file_id = block;
  inode->dir_file_id = parent_dir_block;
  inode->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  inode->content_inode_info.block_current = block;
  inode->content_inode_info.block_prev = CCOS_INVALID_BLOCK;

  for (int i = 0; i < MAX_BLOCKS_IN_INODE; ++i) {
    inode->content_blocks[i] = CCOS_CONTENT_BLOCKS_END_MARKER;
  }

  ccos_update_checksums(inode);
  return inode;
}

ccos_content_inode_t* ccos_get_last_content_inode(const ccos_inode_t* file, const uint8_t* image_data) {
  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    ccos_content_inode_t* result = ccos_get_content_inode(file->content_inode_info.block_next, image_data);
    while (result->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
      result = ccos_get_content_inode(result->content_inode_info.block_next, image_data);
    }

    return result;
  }

  return NULL;
}

ccos_content_inode_t* ccos_add_content_inode(ccos_inode_t* file, uint8_t* data, uint16_t** empty_blocks,
                                             size_t* empty_blocks_size) {
  ccos_block_data_t* content_inode_info = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = ccos_get_last_content_inode(file, data);
  if (last_content_inode != NULL) {
    content_inode_info = &(last_content_inode->content_inode_info);
  }

  if (*empty_blocks_size == 0) {
    fprintf(stderr, "Unable to allocate new content inode: No free space!\n");
    return NULL;
  }

  uint16_t empty_block = (*empty_blocks)[0];
  *empty_blocks = &((*empty_blocks)[1]);
  --*empty_blocks_size;

  uint32_t address = empty_block * BLOCK_SIZE;
  ccos_content_inode_t* content_inode = (ccos_content_inode_t*)&(data[address]);

  content_inode->content_inode_info.header.file_id = content_inode_info->header.file_id;
  content_inode->content_inode_info.header.file_fragment_index = content_inode_info->header.file_fragment_index;
  content_inode->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  content_inode->content_inode_info.block_current = empty_block;
  content_inode->content_inode_info.block_prev = content_inode_info->block_current;

  content_inode_info->block_next = empty_block;

  ccos_update_content_inode_checksums(content_inode);
  if (last_content_inode != NULL) {
    ccos_update_content_inode_checksums(last_content_inode);
  } else {
    ccos_update_checksums(file);
  }

  return content_inode;
}

int ccos_erase_block(uint16_t block, uint8_t* image) {
  uint32_t address = block * BLOCK_SIZE;
  memset(&image[address], 0, BLOCK_SIZE);
  *(uint32_t*)&(image[address]) = CCOS_EMPTY_BLOCK_MARKER;
  return 0;
}

int ccos_remove_content_inode(ccos_inode_t* file, uint8_t* data) {
  if (file->content_inode_info.block_next == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to remove content inode: no content inodes found in file %*s (0x%x)!\n", file->name_length,
            file->name, file->header.file_id);
    return -1;
  }

  ccos_content_inode_t* prev_inode = NULL;
  ccos_block_data_t* prev_block_data = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = ccos_get_content_inode(file->content_inode_info.block_next, data);
  while (last_content_inode->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    prev_inode = last_content_inode;
    prev_block_data = &(prev_inode->content_inode_info);
    last_content_inode = ccos_get_content_inode(last_content_inode->content_inode_info.block_next, data);
  }

  ccos_erase_block(last_content_inode->content_inode_info.block_current, data);

  prev_block_data->block_next = CCOS_INVALID_BLOCK;
  if (prev_inode != NULL) {
    ccos_update_content_inode_checksums(prev_inode);
  } else {
    ccos_update_checksums(file);
  }

  return 0;
}

// remove last content block from the file
int ccos_remove_block_from_file(uint16_t block, uint8_t* data) {
  ccos_inode_t* inode = (ccos_inode_t*)ccos_get_inode(block, data);

  uint16_t* content_blocks = inode->content_blocks;
  ccos_content_inode_t* last_content_inode = ccos_get_last_content_inode(inode, data);
  int content_blocks_count = MAX_BLOCKS_IN_INODE;
  if (last_content_inode != NULL) {
    content_blocks = last_content_inode->content_blocks;
    content_blocks_count = MAX_BLOCKS_IN_CONTENT_INODE;
  }

  uint16_t last_content_block = CCOS_INVALID_BLOCK;
  int last_content_block_index = 0;
  for (; last_content_block_index < content_blocks_count; ++last_content_block_index) {
    if (content_blocks[last_content_block_index] == CCOS_INVALID_BLOCK) {
      if (last_content_block_index > 0) {
        last_content_block = content_blocks[last_content_block_index - 1];
      } else {
        TRACE("File 0x%hx does not have content blocks yet!", block);
      }

      break;
    }
  }

  if (last_content_block_index == content_blocks_count) {
    last_content_block = content_blocks[last_content_block_index - 1];
  }

  if (last_content_block != CCOS_INVALID_BLOCK) {
    ccos_erase_block(last_content_block, data);
    *(uint16_t*)&(content_blocks[last_content_block_index - 1]) = CCOS_INVALID_BLOCK;
  }

  if (last_content_block_index <= 1) {
    if (ccos_remove_content_inode(inode, data) == -1) {
      fprintf(stderr, "Unable to remove content inode ifter freeing block at file 0x%x!\n", block);
      return -1;
    }
  }

  if (last_content_inode != NULL) {
    ccos_update_content_inode_checksums(last_content_inode);
  } else {
    ccos_update_checksums(inode);
  }

  return 0;
}

// get new block from empty blocks, modify it's header properly, reference it in the inode
uint16_t ccos_add_block_to_file(uint16_t block, uint8_t* data, uint16_t** empty_blocks, size_t* empty_blocks_size) {
  if (*empty_blocks_size == 0) {
    fprintf(stderr, "Unable to allocate new content block: No free space!\n");
    return CCOS_INVALID_BLOCK;
  }

  ccos_inode_t* inode = (ccos_inode_t*)ccos_get_inode(block, data);
  ccos_content_inode_t* last_content_inode = NULL;

  uint16_t* content_blocks = inode->content_blocks;
  int content_blocks_count = MAX_BLOCKS_IN_INODE;
  if (inode->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has content inode!");
    last_content_inode = ccos_get_last_content_inode(inode, data);
    content_blocks = last_content_inode->content_blocks;
    content_blocks_count = MAX_BLOCKS_IN_CONTENT_INODE;
  }

  uint16_t last_content_block = 0;
  int last_content_block_index = 0;
  TRACE("%x (%*s): %d content blocks", block, inode->name_length, inode->name, content_blocks_count);
  for (; last_content_block_index < content_blocks_count; ++last_content_block_index) {
    if (content_blocks[last_content_block_index] == CCOS_INVALID_BLOCK) {
      if (last_content_block_index > 0) {
        last_content_block = content_blocks[last_content_block_index - 1];
      } else {
        TRACE("File 0x%hx does not have content blocks yet!", block);
        last_content_block = CCOS_INVALID_BLOCK;
      }

      break;
    }
  }

  if (last_content_block_index == content_blocks_count) {
    last_content_block = content_blocks[last_content_block_index - 1];
  }

  uint16_t new_block = (*empty_blocks)[0];
  *empty_blocks = &((*empty_blocks)[1]);
  --*empty_blocks_size;
  TRACE("Allocating content block 0x%x for file id 0x%x.", new_block, block);
  TRACE("Last content block is 0x%x", last_content_block);
  uint32_t new_block_address = new_block * BLOCK_SIZE;
  ccos_block_header_t* new_block_header = (ccos_block_header_t*)&(data[new_block_address]);
  new_block_header->file_id = inode->header.file_id;

  if (last_content_block != CCOS_INVALID_BLOCK) {
    uint32_t last_block_address = last_content_block * BLOCK_SIZE;
    ccos_block_header_t* last_block_header = (ccos_block_header_t*)&(data[last_block_address]);
    TRACE("Last content block of %hx is %hx with header 0x%hx 0x%hx.", block, last_content_block,
          last_block_header->file_id, last_block_header->file_fragment_index);
    new_block_header->file_fragment_index = last_block_header->file_fragment_index + 1;
  } else {
    new_block_header->file_fragment_index = 0;
  }

  TRACE("New block header: %04x:%04x", new_block_header->file_id, new_block_header->file_fragment_index);

  if (last_content_block_index == content_blocks_count) {
    TRACE("Allocating new content inode for 0x%x...", block);
    // we're run out of space for content blocks; we should allocate next content inode

    ccos_content_inode_t* new_content_inode = ccos_add_content_inode(inode, data, empty_blocks, empty_blocks_size);
    if (new_content_inode == NULL) {
      fprintf(stderr, "Unable to append new content inode to the file: no free space!\n");
      return CCOS_INVALID_BLOCK;
    }

    last_content_inode = new_content_inode;
    content_blocks = new_content_inode->content_blocks;
    content_blocks_count = MAX_BLOCKS_IN_CONTENT_INODE;
    last_content_block_index = 0;
  }

  // append new content block to the list; mark next block in the list as invalid; update checksum;
  content_blocks[last_content_block_index] = new_block;
  TRACE("Content block at %d is now 0x%x.", last_content_block_index, content_blocks[last_content_block_index]);
  if (last_content_block_index < content_blocks_count) {
    content_blocks[last_content_block_index + 1] = CCOS_INVALID_BLOCK;
  }

  ccos_update_checksums(inode);
  if (last_content_inode != NULL) {
    ccos_update_content_inode_checksums(last_content_inode);
  }

  return new_block;
}

int ccos_read_file(uint16_t block, const uint8_t* image_data, uint8_t** file_data, size_t* file_size) {
  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (ccos_get_file_blocks(block, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file at block 0x%x!\n", block);
    return -1;
  }

  ccos_inode_t* file = ccos_get_inode(block, image_data);
  *file_size = file->file_size;
  // Probably work-around for compatibility with older CCOS releases?
  // In some cases, inode->dir_length != inode->file_size, e.g. root dir might have file_size = 0x1F8 bytes (maximum
  // size of a file with one content block), and dir_length = 0xD8 (just some number below 0x1F8). In those cases the
  // correct number is dir_length. But sometimes inode->file_size may be > 0x1F8 (e.g file consists of two blocks) and
  // inode->dir_length will be 0xD8. In those cases the correct number is first one.
  if (ccos_is_dir(block, image_data)) {
    if (file->file_size != file->dir_length) {
      fprintf(stderr, "Warn: dir_length != file_size (%d != %d)\n", file->dir_length, file->file_size);
      if (file->file_size <= CCOS_BLOCK_DATA_SIZE) {
        fprintf(stderr, "Fallback to dir_length\n");
        *file_size = file->dir_length;
      } else {
        fprintf(stderr, "Use file_size\n");
      }
    }
  }
  uint32_t written = 0;

  *file_data = (uint8_t*)calloc(*file_size, sizeof(uint8_t));
  if (*file_data == NULL) {
    fprintf(stderr, "Unable to allocate %ld bytes for file id 0x%x!\n", *file_size, file->header.file_id);
    return -1;
  }

  for (int i = 0; i < blocks_count; ++i) {
    const uint8_t* data_start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(blocks[i], image_data, &data_start, &data_size) == -1) {
      fprintf(stderr, "Unable to get data for data block 0x%x, file block 0x%x\n", blocks[i], block);
      free(*file_data);
      return -1;
    }

    size_t copy_size = MIN(*file_size - written, data_size);
    memcpy(&((*file_data)[written]), data_start, copy_size);

    written += copy_size;
  }

  if (written != *file_size) {
    fprintf(stderr, "Warn: File size (%ld) != amount of bytes read (%u) at file 0x%x!\n", *file_size, written,
            file->header.file_id);
  }

  free(blocks);
  return 0;
}

int ccos_write_file(uint16_t block, uint8_t* image_data, size_t image_size, const uint8_t* file_data,
                    size_t file_size) {
  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (ccos_get_file_blocks(block, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file id 0x%x!\n", block);
    return -1;
  }

  free(blocks);

  TRACE("file id 0x%x has %d blocks", block, blocks_count);
  // add extra blocks to the file if it's new size is greater than the old size
  size_t out_blocks_count = (file_size + CCOS_BLOCK_DATA_SIZE - 1) / CCOS_BLOCK_DATA_SIZE;
  if (out_blocks_count != blocks_count) {
    TRACE("But should contain %d", out_blocks_count);
  }
  if (out_blocks_count > blocks_count) {
    TRACE("Adding %d blocks to the file", out_blocks_count - blocks_count);
    size_t free_blocks_count = 0;
    uint16_t* free_blocks = NULL;

    if (ccos_get_free_blocks(image_data, image_size, &free_blocks, &free_blocks_count) == -1) {
      fprintf(stderr, "Unable to find free blocks in the image!\n");
      return -1;
    }

    uint16_t* free_blocks_out = free_blocks;
    for (int i = 0; i < (out_blocks_count - blocks_count); ++i) {
      TRACE("Adding %d / %d...", i + 1, (out_blocks_count - blocks_count));
      if (ccos_add_block_to_file(block, image_data, &free_blocks_out, &free_blocks_count) == CCOS_INVALID_BLOCK) {
        fprintf(stderr, "Unable to allocate more space for the file 0x%x: no space left!\n", block);
        free(free_blocks);
        return -1;
      }
    }

    TRACE("Done writing file.");
    free(free_blocks);
  } else if (out_blocks_count < blocks_count) {
    TRACE("Removing %d blocks from the file", blocks_count - out_blocks_count);
    for (int i = 0; i < (blocks_count - out_blocks_count); ++i) {
      TRACE("Remove %d / %d...", i + 1, (blocks_count - out_blocks_count));
      if (ccos_remove_block_from_file(block, image_data) == -1) {
        fprintf(stderr, "Unable to remove block from file at 0x%x!\n", block);
        return -1;
      }
    }
  }

  if (ccos_get_file_blocks(block, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for the file id 0x%x!\n", block);
    return -1;
  }

  size_t written = 0;
  for (int i = 0; i < blocks_count; ++i) {
    uint8_t* start = NULL;
    size_t data_size = 0;
    ccos_get_block_data(blocks[i], image_data, (const uint8_t**)&start, &data_size);

    size_t copy_size = MIN(file_size - written, data_size);
    memcpy(start, &(file_data[written]), copy_size);
    written += copy_size;
  }

  if (written != file_size) {
    fprintf(stderr, "Warn: File size (%ld) != amount of bytes read (%ld) at file 0x%x!\n", file_size, written, block);
  }

  free(blocks);

  ccos_inode_t* inode = ccos_get_inode(block, image_data);
  if (ccos_is_dir(block, image_data)) {
    TRACE("Updating dir_length for %*s as well", inode->name_length, inode->name);
    inode->dir_length = written;
  }
  inode->file_size = written;
  ccos_update_checksums(inode);
  return 0;
}

// find a place for the new filename in dir contents (all files are located there in alphabetical, case-insensitive
// order), and insert it there
static int add_file_entry_to_dir_contents(ccos_inode_t* directory, uint8_t* image_data, size_t image_size,
                                          ccos_inode_t* file) {
  uint16_t dir_id = directory->header.file_id;
  size_t dir_size = 0;
  uint8_t* dir_contents = NULL;

  TRACE("Reading contents of the directory %*s (0x%x)", directory->name_length, directory->name, dir_id);
  if (ccos_read_file(dir_id, image_data, &dir_contents, &dir_size) == -1) {
    fprintf(stderr, "Unable to read directory contents at directory id 0x%x\n", dir_id);
    return -1;
  }

  char basename[CCOS_MAX_FILE_NAME] = {0};
  char type[CCOS_MAX_FILE_NAME] = {0};
  size_t basename_length = 0;
  size_t type_length = 0;
  ccos_parse_inode_name(file, basename, type, &basename_length, &type_length);

  char entry_name[CCOS_MAX_FILE_NAME] = {0};
  char entry_type[CCOS_MAX_FILE_NAME] = {0};
  size_t entry_name_length = 0;
  size_t entry_type_length = 0;

  int add_at_last = 0;
  int offset = CCOS_DIR_ENTRIES_OFFSET;
  int parsed_entries = 0;
  for (;;) {
    TRACE("Parsing entry #%d...", parsed_entries);
    dir_entry_t* entry = (dir_entry_t*)&(dir_contents[offset]);
    TRACE("entry block: 0x%x, name length: %d", entry->block, entry->name_length);

    memset(entry_name, 0, CCOS_MAX_FILE_NAME);
    memset(entry_type, 0, CCOS_MAX_FILE_NAME);
    ccos_parse_file_name((const short_string_t*)&(entry->name_length), entry_name, entry_type, &entry_name_length,
                         &entry_type_length);

    TRACE("%s", entry_name);

    // Compare filename and filetype separately
    int res = strncasecmp(entry_name, basename, MIN(entry_name_length, basename_length));
    TRACE("%s %s %s", entry_name, res < 0 ? "<" : res > 0 ? ">" : "==", basename);
    if (res == 0) {
      res = strncasecmp(entry_type, type, MIN(entry_type_length, type_length));
      TRACE("%s %s %s", entry_type, res < 0 ? "<" : res > 0 ? ">" : "==", type);
    }

    if (res > 0) {
      break;
    } else if (res == 0) {
      // TODO: add option to overwrite existing file
      fprintf(stderr, "Unable to add file %*s to the directory: File exists!\n", file->name_length, file->name);
      free(dir_contents);
      return -1;
    } else {
      uint16_t file_suffix = *(uint16_t*)&(dir_contents[offset + sizeof(dir_entry_t) + entry->name_length]);
      TRACE("File suffix: %x", file_suffix);
      offset += (sizeof(dir_entry_t) + entry->name_length + sizeof(file_suffix));
      TRACE("Offset = %d", offset);

      if ((file_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
        TRACE("File %*s should be placed at the end of the directory", file->name_length, file->name);
        add_at_last = 1;
        // removing last entry marker in this case
        *(uint16_t*)&(dir_contents[offset - sizeof(uint16_t)]) &= ~CCOS_DIR_LAST_ENTRY_MARKER;
        break;
      }
    }

    parsed_entries++;
  }

  //  |  XX XX   |   XX    | XX XX XX ... XX |     XX     |    XX     |
  //  |    dir_entry_t     |                 |       file_suffix      |
  //  | file id  |  name   |      name       |    name    | end of    |
  //  |          | length  |                 | length + 3 | dir flag  |
  dir_entry_t dir_entry = {file->header.file_id, file->name_length};
  uint8_t total_size = file->name_length + sizeof(dir_entry_t);
  total_size += sizeof(total_size);
  uint16_t file_suffix = total_size | (add_at_last ? CCOS_DIR_LAST_ENTRY_MARKER : 0);
  size_t file_entry_size = total_size + 1;

  uint8_t* new_file_entry = (uint8_t*)calloc(file_entry_size, sizeof(uint8_t));
  if (new_file_entry == NULL) {
    fprintf(stderr, "Unable to allocate %d bytes for new file entry: %s!\n", total_size + 1, strerror(errno));
    free(dir_contents);
    return -1;
  }

  memcpy(new_file_entry, &dir_entry, sizeof(dir_entry_t));
  memcpy(new_file_entry + sizeof(dir_entry_t), file->name, file->name_length);
  memcpy(new_file_entry + sizeof(dir_entry_t) + file->name_length, &file_suffix, sizeof(uint16_t));

  size_t new_dir_size = dir_size + file_entry_size;
  uint8_t* new_dir_contents = realloc(dir_contents, new_dir_size);
  if (new_dir_contents == NULL) {
    fprintf(stderr, "Unable to realloc %ld bytes for the directory contents: %s!\n", new_dir_size, strerror(errno));
    free(dir_contents);
    free(new_file_entry);
    return -1;
  } else {
    dir_contents = new_dir_contents;
  }

  memmove(dir_contents + offset + file_entry_size, dir_contents + offset, dir_size - offset);
  memcpy(dir_contents + offset, new_file_entry, file_entry_size);
  free(new_file_entry);

  int res = ccos_write_file(directory->header.file_id, image_data, image_size, dir_contents, new_dir_size);
  free(dir_contents);
  if (res == -1) {
    fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", directory->header.file_id);
    return -1;
  }

  return 0;
}

int ccos_add_file_to_directory(ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size) {
  if (add_file_entry_to_dir_contents(directory, image_data, image_size, file) == -1) {
    fprintf(stderr, "Unable to add file with id 0x%x to directory with id 0x%x!\n", file->header.file_id,
            directory->header.file_id);
    return -1;
  }

  file->dir_file_id = directory->header.file_id;
  directory->dir_count += 1;

  ccos_update_checksums(file);
  ccos_update_checksums(directory);

  return 0;
}

// allocate block for the new file inode; copy file inode over; write file contents to the new file; add new file to the
// directory
int ccos_copy_file(uint8_t* dest_image, size_t dest_image_size, ccos_inode_t* dest_directory, const uint8_t* src_image,
                   const ccos_inode_t* src_file) {
  uint16_t* free_blocks = NULL;
  size_t free_blocks_count = 0;
  if (ccos_get_free_blocks(dest_image, dest_image_size, &free_blocks, &free_blocks_count) == -1) {
    fprintf(stderr, "Unable to get block map of an image!\n");
    return -1;
  }

  if (free_blocks_count == 0) {
    fprintf(stderr, "Unable to copy file: no space left!\n");
    free(free_blocks);
    return -1;
  }

  uint16_t new_file_block = free_blocks[0];
  free(free_blocks);

  ccos_inode_t* new_file = ccos_create_inode(new_file_block, dest_directory->header.file_id, dest_image);

  uint8_t* file_data = NULL;
  size_t file_size = 0;
  TRACE("Reading file 0x%lx (%*s)", src_file->header.file_id, src_file->name_length, src_file->name);
  if (ccos_read_file(src_file->header.file_id, src_image, &file_data, &file_size) == -1) {
    fprintf(stderr, "Unable to read source file with id 0x%x!\n", src_file->header.file_id);
    return -1;
  }

  ccos_inode_t* new_inode = ccos_get_inode(new_file_block, dest_image);
  TRACE("Copying file info over...");
  memcpy(&(new_inode->file_size), &(src_file->file_size),
         offsetof(ccos_inode_t, content_inode_info) - offsetof(ccos_inode_t, file_size));

  TRACE("Writing file 0x%lx", new_file->header.file_id);
  if (ccos_write_file(new_file_block, dest_image, dest_image_size, file_data, file_size) == -1) {
    fprintf(stderr, "Unable to write file to file with id 0x%x!\n", new_file_block);
    free(file_data);
    return -1;
  }

  int res = ccos_add_file_to_directory(dest_directory, new_file, dest_image, dest_image_size);
  free(file_data);

  if (res == -1) {
    fprintf(stderr, "Unable to copy file: unable to add new file with id 0x%x to the directory with id 0x%x!\n",
            new_file->header.file_id, dest_directory->header.file_id);
  }

  return res;
}

// - Find parent directory
//    - Remove filename from its contents
//    - Reduce directory size
//    - Reduce directory entry count
//    - Update directory checksums
// - Find all file blocks; clear them and mark as free
// - Clear all file content inode blocks and mark as free
int ccos_delete_file(uint8_t* image, size_t image_size, ccos_inode_t* file) {
  uint16_t parent_dir_id = file->dir_file_id;
  size_t dir_size = 0;
  uint8_t* dir_contents = NULL;

  ccos_inode_t* directory = ccos_get_inode(parent_dir_id, image);
  TRACE("Reading contents of the directory %*s (0x%x)", directory->name_length, directory->name, parent_dir_id);
  if (ccos_read_file(parent_dir_id, image, &dir_contents, &dir_size) == -1) {
    fprintf(stderr, "Unable to read directory contents at directory id 0x%x\n", parent_dir_id);
    return -1;
  }

  char basename[CCOS_MAX_FILE_NAME] = {0};
  char type[CCOS_MAX_FILE_NAME] = {0};
  size_t basename_length = 0;
  size_t type_length = 0;
  ccos_parse_inode_name(file, basename, type, &basename_length, &type_length);

  char entry_name[CCOS_MAX_FILE_NAME] = {0};
  char entry_type[CCOS_MAX_FILE_NAME] = {0};
  size_t entry_name_length = 0;
  size_t entry_type_length = 0;

  int offset = CCOS_DIR_ENTRIES_OFFSET;
  size_t entry_size = 0;
  int parsed_entries = 0;

  uint16_t* prev_file_suffix = NULL;
  uint16_t* file_suffix = NULL;

  for (;;) {
    TRACE("Parsing entry #%d...", parsed_entries++);
    dir_entry_t* entry = (dir_entry_t*)&(dir_contents[offset]);
    TRACE("entry block: 0x%x, name length: %d", entry->block, entry->name_length);

    memset(entry_name, 0, CCOS_MAX_FILE_NAME);
    memset(entry_type, 0, CCOS_MAX_FILE_NAME);
    ccos_parse_file_name((const short_string_t*)&(entry->name_length), entry_name, entry_type, &entry_name_length,
                         &entry_type_length);
    TRACE("%s (%d)", entry_name, entry_name_length);

    // Compare filename and filetype separately
    int res = strncasecmp(entry_name, basename, MIN(entry_name_length, basename_length));
    TRACE("%s %s %s", entry_name, res < 0 ? "<" : res > 0 ? ">" : "==", basename);
    if (res == 0) {
      res = strncasecmp(entry_type, type, MIN(entry_type_length, type_length));
      TRACE("%s %s %s", entry_type, res < 0 ? "<" : res > 0 ? ">" : "==", type);
    }

    prev_file_suffix = file_suffix;
    file_suffix = (uint16_t*)&(dir_contents[offset + sizeof(dir_entry_t) + entry->name_length]);
    TRACE("File suffix: %x", *file_suffix);
    entry_size = sizeof(dir_entry_t) + entry->name_length + sizeof(uint16_t);

    if (res == 0) {
      TRACE("File is found!");
      break;
    } else if (res < 0) {
      if ((*file_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
        fprintf(stderr, "Unable to find file \"%*s\" in directory \"%*s\"!\n", file->name_length, file->name,
                directory->name_length, directory->name);
        free(dir_contents);
        return -1;
      }

      offset += entry_size;
      TRACE("Offset = %d", offset);
    } else {
      fprintf(stderr, "Unable to find file \"%*s\" in directory \"%*s\"!\n", file->name_length, file->name,
              directory->name_length, directory->name);
      free(dir_contents);
      return -1;
    }
  }

  // If we remove last entry, mark the one before it as last.
  if ((*file_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
    if (prev_file_suffix != NULL) {
      *prev_file_suffix = *prev_file_suffix | CCOS_DIR_LAST_ENTRY_MARKER;
    }
  }

  memmove(dir_contents + offset, dir_contents + offset + entry_size, dir_size - (offset + entry_size));

  // Zero last bytes at the end of dir contents. It's not necessary if you have last entry marker set correctly, but
  // it'll help read image in HEX editor if removed dir entry will be nice and zeroed.
  memset(dir_contents + dir_size - entry_size, 0, entry_size);
  size_t new_dir_size = dir_size - entry_size;

  // Write dir contents back with old size to overwrite bytes at the end of dir with zeroes.
  int res = ccos_write_file(directory->header.file_id, image, image_size, dir_contents, dir_size);

  // Do that once more with new size to clear freed up content block.
  if (res != -1) {
    res = ccos_write_file(directory->header.file_id, image, image_size, dir_contents, new_dir_size);
  }

  free(dir_contents);
  if (res == -1) {
    fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", directory->header.file_id);
    return -1;
  }

  directory->file_size = new_dir_size;
  directory->dir_length = new_dir_size;
  directory->dir_count = directory->dir_count - 1;

  ccos_update_checksums(directory);

  size_t blocks_count = 0;
  uint16_t* blocks = NULL;
  if (ccos_get_file_blocks(file->header.file_id, image, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to read file blocks of file %*s (0x%x)!\n", file->name_length, file->name,
            file->header.file_id);
    return -1;
  }

  for (int i = 0; i < blocks_count; ++i) {
    ccos_erase_block(blocks[i], image);
  }
  free(blocks);

  while (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    if (ccos_remove_content_inode(file, image) == -1) {
      fprintf(stderr, "Unable to remove content block from the file %*s (0x%x)!\n", file->name_length, file->name,
              file->header.file_id);
      return -1;
    }
  }

  ccos_erase_block(file->header.file_id, image);

  return 0;
}
