//
// Created by kirill on 21.05.2020.
//

#include "ccos_private.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/**
 * Superblock is a block which contains root directory description. Usually CCOS disc image contains superblock number
 * at the offset 0x20. Sometimes though, it doesn't. In such cases we assume that superblock is # 0x121.
 */
#define CCOS_SUPERBLOCK_ADDR_OFFSET 0x20
#define CCOS_DEFAULT_SUPERBLOCK 0x121

#define CCOS_CONTENT_BLOCKS_END_MARKER 0xFFFF

uint16_t calc_checksum(const uint8_t* data, uint16_t data_size) {
  uint16_t ret = 0;
  for (int i = 0; i < data_size; i += 2) {
    if (i + 2 > data_size) {
      ret += data[0];
    } else {
      ret += (uint16_t)(data[1] << 8u) | (data[0]);
    }
    data += 2;
  }

  return ret;
}

uint16_t calc_inode_metadata_checksum(const ccos_inode_t* inode) {
  return calc_checksum((const uint8_t*)&(inode->header), offsetof(ccos_inode_t, metadata_checksum));
}

uint16_t calc_inode_blocks_checksum(const ccos_inode_t* inode) {
  uint16_t blocks_checksum =
      calc_checksum((const uint8_t*)&(inode->content_inode_info.block_next),
                    offsetof(ccos_inode_t, block_end) - offsetof(ccos_inode_t, content_inode_info) -
                        offsetof(ccos_block_data_t, block_next));
  blocks_checksum += inode->content_inode_info.header.file_id;
  blocks_checksum += inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
}

uint16_t calc_content_inode_checksum(const ccos_content_inode_t* content_inode) {
  uint16_t blocks_checksum =
      calc_checksum((const uint8_t*)&(content_inode->content_inode_info.block_next),
                    offsetof(ccos_content_inode_t, block_end) - offsetof(ccos_content_inode_t, content_inode_info) -
                        offsetof(ccos_block_data_t, block_next));
  blocks_checksum += content_inode->content_inode_info.header.file_id;
  blocks_checksum += content_inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
}

uint16_t calc_bitmask_checksum(const ccos_bitmask_t* bitmask) {
  uint16_t checksum = calc_checksum((uint8_t*)&(bitmask->allocated), BITMASK_SIZE + sizeof(bitmask->allocated));
  checksum += bitmask->header.file_id;
  checksum += bitmask->header.file_fragment_index;
  return checksum;
}

void update_inode_checksums(ccos_inode_t* inode) {
  inode->metadata_checksum = calc_inode_metadata_checksum(inode);
  inode->content_inode_info.blocks_checksum = calc_inode_blocks_checksum(inode);
}

void update_content_inode_checksums(ccos_content_inode_t* content_inode) {
  content_inode->content_inode_info.blocks_checksum = calc_content_inode_checksum(content_inode);
}

void update_bitmask_checksum(ccos_bitmask_t* bitmask) {
  bitmask->checksum = calc_bitmask_checksum(bitmask);
}

ccos_inode_t* get_inode(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  return (ccos_inode_t*)&(data[addr]);
}

ccos_content_inode_t* get_content_inode(uint16_t block, const uint8_t* data) {
  uint32_t addr = block * BLOCK_SIZE;
  return (ccos_content_inode_t*)&(data[addr]);
}

int get_superblock(const uint8_t* data, size_t image_size, uint16_t* superblock) {
  uint16_t res = *((uint16_t*)&(data[CCOS_SUPERBLOCK_ADDR_OFFSET]));
  if (res == 0) {
    res = CCOS_DEFAULT_SUPERBLOCK;
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
  TRACE("superblock: 0x%x", *superblock);
  return 0;
}

int get_file_blocks(const ccos_inode_t* file, const uint8_t* data, size_t* blocks_count, uint16_t** blocks) {
  *blocks = calloc(MAX_BLOCKS_IN_INODE, sizeof(uint16_t));
  if (*blocks == NULL) {
    return -1;
  }

  size_t real_blocks_count = 0;
  for (int i = 0; i < MAX_BLOCKS_IN_INODE; ++i) {
    uint16_t content_block = file->content_blocks[i];
    if (content_block == CCOS_CONTENT_BLOCKS_END_MARKER) {
      break;
    }

    (*blocks)[real_blocks_count++] = content_block;
  }

  TRACE("Block count in 0x%lx itself: %d", file->header.file_id, real_blocks_count);

  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has more than 1 block!");
    const ccos_content_inode_t* content_inode = get_content_inode(file->content_inode_info.block_next, data);
    for (;;) {
      TRACE("Processing extra block 0x%lx...", file->content_inode_info.block_next);

      uint16_t checksum = calc_content_inode_checksum(content_inode);

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

      content_inode = get_content_inode(content_inode->content_inode_info.block_next, data);
    }
  }

  *blocks_count = real_blocks_count;
  return 0;
}

uint16_t get_bitmask_block(uint16_t superblock) {
  if (superblock > 0) {
    return superblock - 1;
  } else {
    fprintf(stderr, "Unable to get bitmask block: invalid superblock %x!\n", superblock);
    return CCOS_INVALID_BLOCK;
  }
}

ccos_bitmask_t* get_bitmask(uint8_t* data, size_t data_size) {
  uint16_t superblock = CCOS_INVALID_BLOCK;
  if (get_superblock(data, data_size, &superblock) == -1) {
    fprintf(stderr, "Unable to get image bitmask: Invalid superblock!\n");
    return NULL;
  }

  uint16_t bitmask_block = get_bitmask_block(superblock);
  if (bitmask_block == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to get image bitmask: Invalid bitmask block!\n");
    return NULL;
  }

  uint32_t address = bitmask_block * BLOCK_SIZE;
  return (ccos_bitmask_t*)&(data[address]);
}

uint16_t get_free_block(const ccos_bitmask_t* bitmask) {
  for (int i = 0; i < BITMASK_SIZE; ++i) {
    if (bitmask->bytes[i] != 0xFF) {
      uint8_t byte = bitmask->bytes[i];
      for (int j = 0; j < 8; ++j) {
        if (!(byte & 1u)) {
          return i * 8 + j;
        }

        byte = byte >> 1u;
      }
    }
  }

  return CCOS_INVALID_BLOCK;
}

void mark_block(ccos_bitmask_t* bitmask, uint16_t block, uint8_t mode) {
  TRACE("Mark block %x as %s...", block, mode ? "used" : "free");

  uint8_t* byte = &(bitmask->bytes[block >> 3u]);
  if (mode) {
    *byte = *byte | (1u << (block & 0b111u));
    bitmask->allocated += 1;
  } else {
    *byte = *byte & ~(1u << (block & 0b111u));
    bitmask->allocated -= 1;
  }

  update_bitmask_checksum(bitmask);
}

ccos_inode_t* init_inode(uint16_t block, uint16_t parent_dir_block, uint8_t* image_data) {
  TRACE("Initializing inode at 0x%x!", block);
  ccos_inode_t* inode = get_inode(block, image_data);
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

  update_inode_checksums(inode);
  return inode;
}

ccos_content_inode_t* add_content_inode(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask) {
  ccos_block_data_t* content_inode_info = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = get_last_content_inode(file, data);
  if (last_content_inode != NULL) {
    content_inode_info = &(last_content_inode->content_inode_info);
  }

  uint16_t new_block = get_free_block(bitmask);
  if (new_block == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to allocate new content inode: No free space!\n");
    return NULL;
  }
  mark_block(bitmask, new_block, 1);

  ccos_content_inode_t* content_inode = get_content_inode(new_block, data);

  content_inode->content_inode_info.header.file_id = content_inode_info->header.file_id;
  content_inode->content_inode_info.header.file_fragment_index = content_inode_info->header.file_fragment_index;
  content_inode->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  content_inode->content_inode_info.block_current = new_block;
  content_inode->content_inode_info.block_prev = content_inode_info->block_current;

  content_inode_info->block_next = new_block;

  update_content_inode_checksums(content_inode);
  if (last_content_inode != NULL) {
    update_content_inode_checksums(last_content_inode);
  } else {
    update_inode_checksums(file);
  }

  return content_inode;
}

ccos_content_inode_t* get_last_content_inode(const ccos_inode_t* file, const uint8_t* image_data) {
  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    ccos_content_inode_t* result = get_content_inode(file->content_inode_info.block_next, image_data);
    while (result->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
      result = get_content_inode(result->content_inode_info.block_next, image_data);
    }

    return result;
  }

  return NULL;
}

void erase_block(uint16_t block, uint8_t* image, ccos_bitmask_t* bitmask) {
  uint32_t address = block * BLOCK_SIZE;
  memset(&image[address], 0, BLOCK_SIZE);
  *(uint32_t*)&(image[address]) = CCOS_EMPTY_BLOCK_MARKER;
  mark_block(bitmask, block, 0);
}

int remove_content_inode(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask) {
  if (file->content_inode_info.block_next == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to remove content inode: no content inodes found in file %*s (0x%x)!\n", file->name_length,
            file->name, file->header.file_id);
    return -1;
  }

  ccos_content_inode_t* prev_inode = NULL;
  ccos_block_data_t* prev_block_data = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = get_last_content_inode(file, data);

  erase_block(last_content_inode->content_inode_info.block_current, data, bitmask);

  prev_block_data->block_next = CCOS_INVALID_BLOCK;
  if (prev_inode != NULL) {
    update_content_inode_checksums(prev_inode);
  } else {
    update_inode_checksums(file);
  }

  return 0;
}

// remove last content block from the file
int remove_block_from_file(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask) {
  uint16_t* content_blocks = file->content_blocks;
  ccos_content_inode_t* last_content_inode = get_last_content_inode(file, data);
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
        TRACE("File 0x%hx does not have content blocks yet!", file->header.file_id);
      }

      break;
    }
  }

  if (last_content_block_index == content_blocks_count) {
    last_content_block = content_blocks[last_content_block_index - 1];
  }

  if (last_content_block != CCOS_INVALID_BLOCK) {
    erase_block(last_content_block, data, bitmask);
    *(uint16_t*)&(content_blocks[last_content_block_index - 1]) = CCOS_INVALID_BLOCK;
  }

  if (last_content_block_index <= 1) {
    if (remove_content_inode(file, data, bitmask) == -1) {
      fprintf(stderr, "Unable to remove content inode after freeing block at file 0x%x!\n", file->header.file_id);
      return -1;
    }
  }

  if (last_content_inode != NULL) {
    update_content_inode_checksums(last_content_inode);
  } else {
    update_inode_checksums(file);
  }

  return 0;
}

// get new block from empty blocks, modify it's header properly, reference it in the inode
uint16_t add_block_to_file(ccos_inode_t* file, uint8_t* data, ccos_bitmask_t* bitmask) {
  ccos_content_inode_t* last_content_inode = NULL;

  uint16_t* content_blocks = file->content_blocks;
  int content_blocks_count = MAX_BLOCKS_IN_INODE;
  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has content inode!");
    last_content_inode = get_last_content_inode(file, data);
    content_blocks = last_content_inode->content_blocks;
    content_blocks_count = MAX_BLOCKS_IN_CONTENT_INODE;
  }

  uint16_t last_content_block = 0;
  int last_content_block_index = 0;
  TRACE("%x (%*s): %d content blocks", file->header.file_id, file->name_length, file->name, content_blocks_count);
  for (; last_content_block_index < content_blocks_count; ++last_content_block_index) {
    if (content_blocks[last_content_block_index] == CCOS_INVALID_BLOCK) {
      if (last_content_block_index > 0) {
        last_content_block = content_blocks[last_content_block_index - 1];
      } else {
        TRACE("File 0x%hx does not have content blocks yet!", file->header.file_id);
        last_content_block = CCOS_INVALID_BLOCK;
      }

      break;
    }
  }

  if (last_content_block_index == content_blocks_count) {
    last_content_block = content_blocks[last_content_block_index - 1];
  }

  uint16_t new_block = get_free_block(bitmask);
  if (new_block == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to allocate new content block: No free space!\n");
    return CCOS_INVALID_BLOCK;
  }

  TRACE("Allocating content block 0x%x for file id 0x%x.", new_block, file->header.file_id);
  mark_block(bitmask, new_block, 1);

  TRACE("Last content block is 0x%x", last_content_block);
  uint32_t new_block_address = new_block * BLOCK_SIZE;
  ccos_block_header_t* new_block_header = (ccos_block_header_t*)&(data[new_block_address]);
  new_block_header->file_id = file->header.file_id;

  if (last_content_block != CCOS_INVALID_BLOCK) {
    uint32_t last_block_address = last_content_block * BLOCK_SIZE;
    ccos_block_header_t* last_block_header = (ccos_block_header_t*)&(data[last_block_address]);
    TRACE("Last content block of %hx is %hx with header 0x%hx 0x%hx.", file->header.file_id, last_content_block,
          last_block_header->file_id, last_block_header->file_fragment_index);
    new_block_header->file_fragment_index = last_block_header->file_fragment_index + 1;
  } else {
    new_block_header->file_fragment_index = 0;
  }

  TRACE("New block header: %04x:%04x", new_block_header->file_id, new_block_header->file_fragment_index);

  if (last_content_block_index == content_blocks_count) {
    TRACE("Allocating new content inode for 0x%x...", file->header.file_id);
    // we're run out of space for content blocks; we should allocate next content inode

    ccos_content_inode_t* new_content_inode = add_content_inode(file, data, bitmask);
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

  update_inode_checksums(file);
  if (last_content_inode != NULL) {
    update_content_inode_checksums(last_content_inode);
  }

  return new_block;
}

int add_file_to_directory(ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size) {
  if (add_file_entry_to_dir_contents(directory, image_data, image_size, file) == -1) {
    fprintf(stderr, "Unable to add file with id 0x%x to directory with id 0x%x!\n", file->header.file_id,
            directory->header.file_id);
    return -1;
  }

  file->dir_file_id = directory->header.file_id;
  directory->dir_count += 1;

  update_inode_checksums(file);
  update_inode_checksums(directory);

  return 0;
}

int parse_directory_data(uint8_t* image_data, const uint8_t* directory_data, size_t directory_data_size,
                         uint16_t entry_count, parsed_directory_element_t** entries) {
  *entries = (parsed_directory_element_t*)calloc(entry_count, sizeof(parsed_directory_element_t));
  if (*entries == NULL) {
    return -1;
  }

  TRACE("Parsing %d dir entries, size = %d bytes...", entry_count, directory_data_size);

  size_t offset = 0;
  for (uint16_t count = 0; count < entry_count && offset < directory_data_size; count++) {
    if (directory_data[offset] == CCOS_DIR_LAST_ENTRY_MARKER) {
      TRACE("Last directory entry found after parsing %d entries.", count);
      break;
    }

    offset += sizeof(uint8_t);

    TRACE("%d / %d, offset = %d bytes...", count + 1, entry_count, offset);
    dir_entry_t* entry = (dir_entry_t*)&(directory_data[offset]);

    TRACE("entry block: 0x%x, name length: %d characters", entry->block, entry->name_length);
    uint16_t entry_block = entry->block;
    uint8_t reverse_length = *(uint8_t*)&(directory_data[offset + sizeof(dir_entry_t) + entry->name_length]);
    size_t entry_size = sizeof(dir_entry_t) + entry->name_length + sizeof(reverse_length);

    (*entries)[count].offset = offset;
    (*entries)[count].size = entry_size;
    (*entries)[count].file = get_inode(entry_block, image_data);

    offset += entry_size;
  }

  return 0;
}

// Create new directory entry:
//
// offset |  00  01  |   02    | 03 04 05 ... NN |    NN+1    |    NN+2    |
// -------|----------|---------|-----------------|------------|------------|
//        | file id  |  name   |      name       |    NN+1    | last entry |
//        |          | length  |                 |            |    flag    |
static int create_directory_entry(ccos_inode_t* file, int is_last, size_t* entry_size, uint8_t** directory_entry) {
  uint8_t reverse_length = file->name_length + sizeof(dir_entry_t) + sizeof(reverse_length);
  uint8_t last_entry_flag = is_last ? CCOS_DIR_LAST_ENTRY_MARKER : 0;
  *entry_size = reverse_length + sizeof(last_entry_flag);
  *directory_entry = (uint8_t*)calloc(*entry_size, sizeof(uint8_t));
  if (*directory_entry == NULL) {
    fprintf(stderr, "Unable to allocate %d bytes for new directory entry: %s!\n", reverse_length + 1, strerror(errno));
    return -1;
  }

  size_t offset = 0;
  ((uint16_t*)*directory_entry)[offset] = file->header.file_id;
  offset += sizeof(file->header.file_id);
  (*directory_entry)[offset] = file->name_length;
  offset += sizeof(file->name_length);
  memcpy(*directory_entry + offset, file->name, file->name_length);
  offset += file->name_length;
  (*directory_entry)[offset] = reverse_length;
  offset += sizeof(reverse_length);
  (*directory_entry)[offset] = last_entry_flag;
  return 0;
}

// find a place for the new filename in dir contents (all files are located there in alphabetical, case-insensitive
// order), and insert it there
int add_file_entry_to_dir_contents(ccos_inode_t* directory, uint8_t* image_data, size_t image_size,
                                   ccos_inode_t* file) {
  TRACE("Directory size: %d bytes, length: %d, has %d entries", directory->file_size, directory->dir_length,
        directory->dir_count);

  uint8_t* directory_data = NULL;
  size_t dir_size = 0;
  if (ccos_read_file(directory, image_data, &directory_data, &dir_size) == -1) {
    fprintf(stderr, "Unable to get directory contents: Unable to read directory!\n");
    if (directory_data != NULL) {
      free(directory_data);
      return -1;
    }
  }

  parsed_directory_element_t* elements = NULL;
  if (parse_directory_data(image_data, directory_data, dir_size, directory->dir_count, &elements) == -1) {
    fprintf(stderr, "Unable to add file to directory files list: Unable to parse directory data!\n");
    return -1;
  }

  // 1. Find place for the new file to insert
  int i = find_file_index_in_directory_data(file, directory, elements);
  if ((i < directory->dir_count) && (file->name_length == elements[i].file->name_length) &&
      !(strncasecmp(file->name, elements[i].file->name, file->name_length))) {
    // TODO: add option to overwrite existing file
    fprintf(stderr, "Unable to add file %*s to the directory: File exists!\n", file->name_length, file->name);
    free(directory_data);
    return -1;
  }

  int new_entry_is_last = i == directory->dir_count;

  // 2. Create new directory entry
  uint8_t* new_file_entry = NULL;
  size_t file_entry_size = 0;
  if (create_directory_entry(file, new_entry_is_last, &file_entry_size, &new_file_entry) == -1) {
    fprintf(stderr, "Unable to add new entry to the directory: Unable to create entry!\n");
    free(directory_data);
    free(elements);
    return -1;
  }

  // 3. Insert new entry into directory data
  size_t real_dir_size = 1;
  if (directory->dir_count > 0) {
    real_dir_size = elements[directory->dir_count - 1].offset + elements[directory->dir_count - 1].size + 1;
  }

  TRACE("Real directory size: " SIZE_T " bytes", real_dir_size);
  size_t new_dir_size = real_dir_size + file_entry_size;
  TRACE("Dir size " SIZE_T " -> " SIZE_T ".", dir_size, new_dir_size);

  uint8_t* new_directory_data = realloc(directory_data, new_dir_size);
  if (new_directory_data == NULL) {
    fprintf(stderr, "Unable to realloc " SIZE_T " bytes for the directory contents: %s!\n", new_dir_size,
            strerror(errno));
    free(directory_data);
    free(new_file_entry);
    return -1;
  } else {
    directory_data = new_directory_data;
  }

  if (new_entry_is_last) {
    size_t last_entry_offset;
    if (directory->dir_count == 0) {
      last_entry_offset = CCOS_DIR_ENTRIES_OFFSET;
    } else {
      // |  <------------ elements[i-1].size ------------->  |
      // |  .-- elements[i-1].offset                         |
      // |  V                                                |
      // |  00  01  |   02    | 03 04 05 ... NN |    NN+1    |    NN+2    |
      // |----------|---------|-----------------|------------|------------|
      // | file id  |  name   |      name       |  reversed  | last entry |
      // |          | length  |                 |  length    |    flag    |
      last_entry_offset = elements[i - 1].offset + elements[i - 1].size + sizeof(uint8_t);
    }

    memcpy(directory_data + last_entry_offset, new_file_entry, file_entry_size);
  } else {
    memmove(directory_data + elements[i].offset + file_entry_size, directory_data + elements[i].offset,
            dir_size - elements[i].offset);
    memcpy(directory_data + elements[i].offset, new_file_entry, file_entry_size);
  }

  free(new_file_entry);

  // 4. Remove last entry flag from previous last entry if necessary
  if (new_entry_is_last) {
    if (directory->dir_count == 0) {
      // Empty directory contains only last entry flag
      directory_data[0] = 0;
    } else {
      directory_data[elements[i - 1].offset + elements[i - 1].size] = 0;
    }
  }

  // 5. Save changes
  int res = ccos_write_file(directory, image_data, image_size, directory_data, new_dir_size);
  free(directory_data);
  if (res == -1) {
    fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", directory->header.file_id);
    return -1;
  }

  return 0;
}

int find_file_index_in_directory_data(ccos_inode_t* file, ccos_inode_t* directory,
                                      parsed_directory_element_t* elements) {
  char basename[CCOS_MAX_FILE_NAME] = {0};
  char type[CCOS_MAX_FILE_NAME] = {0};
  size_t basename_length = 0;
  size_t type_length = 0;
  ccos_parse_file_name(file, basename, type, &basename_length, &type_length);

  char entry_name[CCOS_MAX_FILE_NAME] = {0};
  char entry_type[CCOS_MAX_FILE_NAME] = {0};
  size_t entry_name_length = 0;
  size_t entry_type_length = 0;

  int i;
  for (i = 0; i < directory->dir_count; ++i) {
    TRACE("Parsing entry # %d...", i);

    memset(entry_name, 0, CCOS_MAX_FILE_NAME);
    memset(entry_type, 0, CCOS_MAX_FILE_NAME);
    parse_file_name((const short_string_t*)&(elements[i].file->name_length), entry_name, entry_type, &entry_name_length,
                    &entry_type_length);

    TRACE("%s", entry_name);

    // Compare filename and file type separately
    int res = strcasecmp(entry_name, basename);
    TRACE("%s %s %s", entry_name, res < 0 ? "<" : res > 0 ? ">" : "==", basename);
    if (res == 0) {
      res = strncasecmp(entry_type, type, MIN(entry_type_length, type_length));
      TRACE("%s %s %s", entry_type, res < 0 ? "<" : res > 0 ? ">" : "==", type);
    }

    if (res >= 0) {
      break;
    }
  }

  return i;
}

int parse_file_name(const short_string_t* file_name, char* basename, char* type, size_t* name_length,
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

int get_block_data(uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size) {
  // TODO: check bounds
  uint32_t address = block * BLOCK_SIZE;
  *start = &(data[address + CCOS_DATA_OFFSET]);
  *size = CCOS_BLOCK_DATA_SIZE;
  return 0;
}

int get_free_blocks(ccos_bitmask_t* bitmask, size_t data_size, size_t* free_blocks_count, uint16_t** free_blocks) {
  size_t free_count = 0;
  size_t block_count = data_size / BLOCK_SIZE;

  uint16_t checksum = calc_bitmask_checksum(bitmask);
  if (bitmask->checksum != checksum) {
    fprintf(stderr, "Warn: bitmask checksum mismatch! Expected: 0x%x, got: 0x%x!\n", bitmask->checksum, checksum);
  }

  TRACE("Allocated: %d, total: %d", bitmask->allocated, block_count);
  *free_blocks_count = block_count - bitmask->allocated;
  TRACE("Free blocks: %d", *free_blocks_count);
  *free_blocks = (uint16_t*)calloc(*free_blocks_count, sizeof(uint16_t));
  if (*free_blocks == NULL) {
    fprintf(stderr, "Unable to allocate " SIZE_T " bytes for free blocks: %s!\n", *free_blocks_count * sizeof(uint16_t),
            strerror(errno));
    return -1;
  }

  for (int i = 0; i < BITMASK_SIZE; ++i) {
    if (bitmask->bytes[i] != 0xFF) {
      uint8_t byte = bitmask->bytes[i];
      for (int j = 0; j < 8; ++j) {
        if (!(byte & 1u)) {
          (*free_blocks)[free_count++] = i * 8 + j;
        }
        byte = byte >> 1u;
      }
    }
  }

  if (free_count != *free_blocks_count) {
    fprintf(stderr, "Warn: free block count (" SIZE_T ") mismatches found free blocks count (" SIZE_T ")!\n",
            *free_blocks_count, free_count);
  }

  return 0;
}

int is_root_dir(ccos_inode_t* file) {
  // In CCOS, root directory's parent file id it its own file id.
  return file->header.file_id == file->dir_file_id;
}
