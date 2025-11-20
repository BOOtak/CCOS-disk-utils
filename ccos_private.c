//
// Created by kirill on 21.05.2020.
//
#include "common.h"
#include "ccos_image.h"
#include "ccos_private.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CCOS_SUPERBLOCK_ADDR_OFFSET 0x20
#define CCOS_BITMASK_ADDR_OFFSET 0x1E

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
  return calc_checksum((const uint8_t*)inode, offsetof(ccos_inode_t, desc) + offsetof(ccos_inode_desc_t, metadata_checksum));
}

uint16_t calc_inode_blocks_checksum(ccfs_handle ctx, const ccos_inode_t* inode) {
  const size_t start_offset = offsetof(ccos_inode_t, content_inode_info) + offsetof(ccos_block_data_t, block_next);

  const uint8_t* checksum_data = (const uint8_t*)&inode->content_inode_info.block_next;
  uint16_t checksum_data_size = get_block_size(ctx) - start_offset;

  uint16_t blocks_checksum = calc_checksum(checksum_data, checksum_data_size);
  blocks_checksum += inode->content_inode_info.header.file_id;
  blocks_checksum += inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
}

uint16_t calc_content_inode_checksum(ccfs_handle ctx, const ccos_content_inode_t* content_inode) {
  const size_t start_offset = offsetof(ccos_content_inode_t, content_inode_info) + offsetof(ccos_block_data_t, block_next);

  const uint8_t *checksum_data = (const uint8_t*)&content_inode->content_inode_info.block_next;
  uint16_t checksum_data_size = get_block_size(ctx) - start_offset - get_content_inode_padding(ctx);

  uint16_t blocks_checksum = calc_checksum(checksum_data, checksum_data_size);
  blocks_checksum += content_inode->content_inode_info.header.file_id;
  blocks_checksum += content_inode->content_inode_info.header.file_fragment_index;

  return blocks_checksum;
}

uint16_t calc_bitmask_checksum(ccfs_handle ctx, const ccos_bitmask_t* bitmask) {
  const uint8_t* checksum_data = (const uint8_t*)&bitmask->allocated;
  uint16_t checksum_data_size = get_bitmask_size(ctx) + sizeof(bitmask->allocated);

  uint16_t checksum = calc_checksum(checksum_data, checksum_data_size);
  checksum += bitmask->header.file_id;
  checksum += bitmask->header.file_fragment_index;
  
  return checksum;
}

void update_inode_checksums(ccfs_handle ctx, ccos_inode_t* inode) {
  inode->desc.metadata_checksum = calc_inode_metadata_checksum(inode);
  inode->content_inode_info.blocks_checksum = calc_inode_blocks_checksum(ctx, inode);
}

void update_content_inode_checksums(ccfs_handle ctx, ccos_content_inode_t* content_inode) {
  content_inode->content_inode_info.blocks_checksum = calc_content_inode_checksum(ctx, content_inode);
}

void update_bitmask_checksum(ccfs_handle ctx, ccos_bitmask_t* bitmask) {
  bitmask->checksum = calc_bitmask_checksum(ctx, bitmask);
}

ccos_inode_t* get_inode(ccfs_handle ctx, uint16_t block, const uint8_t* data) {
  size_t block_size = get_block_size(ctx);
  uint32_t addr = block * block_size;
  return (ccos_inode_t*)&(data[addr]);
}

ccos_content_inode_t* get_content_inode(ccfs_handle ctx, uint16_t block, const uint8_t* data) {
  size_t block_size = get_block_size(ctx);
  uint32_t addr = block * block_size;
  return (ccos_content_inode_t*)&(data[addr]);
}

int get_superblock(ccfs_handle ctx, const uint8_t* data, size_t image_size, uint16_t* superblock) {
  uint16_t sb = *(uint16_t*)&data[CCOS_SUPERBLOCK_ADDR_OFFSET];
  if (sb == 0) sb = ctx->superblock_id;

  size_t block_size = get_block_size(ctx);
  uint32_t blocks_in_image = image_size / block_size;

  if (sb > blocks_in_image) {
    fprintf(stderr, "Invalid superblock! (Superblock: 0x%x, but only 0x%x blocks in the image).\n",
            sb, blocks_in_image);
    return -1;
  }

  uint32_t block_addr = sb * block_size;
  uint16_t block_header = *(uint16_t*)&data[block_addr];
  if (block_header != sb) {
    fprintf(stderr, "Invalid image: Block header 0x%x mismatches superblock 0x%x!\n", block_header, sb);
    return -1;
  }

  *superblock = sb;
  TRACE("superblock: 0x%x", *superblock);
  return 0;
}

int get_file_blocks(ccfs_handle ctx, ccos_inode_t* file, const uint8_t* data, size_t* blocks_count, uint16_t** blocks) {
  size_t inode_max_blocks = get_inode_max_blocks(ctx);
  size_t content_inode_max_blocks = get_content_inode_max_blocks(ctx);

  *blocks = calloc(inode_max_blocks, sizeof(uint16_t));
  if (*blocks == NULL) {
    TRACE("Failed to alloc memory for inode blocks");
    return -1;
  }

  size_t real_blocks_count = 0;
  
  uint16_t* file_content_blocks = get_inode_content_blocks(file);
  for (int i = 0; i < inode_max_blocks; ++i) {
    uint16_t content_block = file_content_blocks[i];
    if (content_block == CCOS_CONTENT_BLOCKS_END_MARKER) {
      continue;
    }

    (*blocks)[real_blocks_count++] = content_block;
  }

  TRACE("Block count in 0x%lx itself: %d", file->header.file_id, real_blocks_count);

  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has more than 1 block!");
    ccos_content_inode_t* content_inode = get_content_inode(ctx, file->content_inode_info.block_next, data);
    for (;;) {
      TRACE("Processing extra block 0x%lx...", file->content_inode_info.block_next);

      uint16_t checksum = calc_content_inode_checksum(ctx, content_inode);

      if (checksum != content_inode->content_inode_info.blocks_checksum) {
        fprintf(stderr, "Warn: Blocks checksum mismatch: expected 0x%04hx, got 0x%04hx\n",
                content_inode->content_inode_info.blocks_checksum, checksum);
      }

      uint16_t* extra_blocks = calloc(content_inode_max_blocks, sizeof(uint16_t));
      if (extra_blocks == NULL) {
        fprintf(stderr, "Unable to allocate memory for extra blocks: %s!\n", strerror(errno));
        free(*blocks);
        return -1;
      }

      uint16_t* content_blocks = get_content_inode_content_blocks(content_inode);

      size_t extra_blocks_count = 0;
      for (int i = 0; i < content_inode_max_blocks; ++i) {
        uint16_t content_block = content_blocks[i];
        if (content_block == CCOS_CONTENT_BLOCKS_END_MARKER) {
          continue;
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

      content_inode = get_content_inode(ctx, content_inode->content_inode_info.block_next, data);
    }
  }

  *blocks_count = real_blocks_count;
  return 0;
}

static ccos_bitmask_t* get_bitmask(ccfs_handle ctx, uint8_t* data, size_t data_size) {
  size_t block_size = get_block_size(ctx);

  uint16_t bitmask_block = *((uint16_t*)&(data[CCOS_BITMASK_ADDR_OFFSET]));
  if (bitmask_block == 0) bitmask_block = ctx->bitmap_block_id;

  uint32_t blocks_in_image = data_size / block_size;
  if (bitmask_block > blocks_in_image) {
    fprintf(stderr, "Invalid bitmask block! (Bitmask: 0x%x, but only 0x%x blocks in the image).\n", bitmask_block,
            blocks_in_image);
    return NULL;
  }

  uint32_t addr = bitmask_block * block_size;
  uint16_t block_header = *(uint16_t*)&(data[addr]);
  if (block_header != bitmask_block) {
    fprintf(stderr, "Invalid image: Block header 0x%x mismatches bitmask 0x%x!\n", block_header, bitmask_block);
    return NULL;
  }

  TRACE("Bitmask: 0x%x", bitmask_block);
  uint32_t address = bitmask_block * block_size;
  return (ccos_bitmask_t*)&(data[address]);
}

ccos_bitmask_list_t find_bitmask_blocks(ccfs_handle ctx, uint8_t* data, size_t data_size) {
  ccos_bitmask_list_t result = {0};
  ccos_bitmask_t* first_bitmask_block = get_bitmask(ctx, data, data_size);
  if (first_bitmask_block == NULL) {
    fprintf(stderr, "Unable to get bitmask blocks: No bitmask in image!\n");
    return result;
  }

  size_t block_size = get_block_size(ctx);

  uint16_t bitmask_id = first_bitmask_block->header.file_id;
  uint32_t bitmask_addr = bitmask_id * block_size;

  for (size_t i = 0; i < MAX_BITMASK_BLOCKS_IN_IMAGE; i++) {
    uint32_t offset = bitmask_addr + i * block_size;
    ccos_block_header_t* header = (ccos_block_header_t*)&(data[offset]);
    if (header->file_id == bitmask_id) {
      if (header->file_fragment_index != i) {
        fprintf(stderr, "WARN: 0x%x: Invalid bitmask fragment index: expected: " SIZE_T "; actual: %d!\n", offset, i,
                header->file_fragment_index);
      }

      result.bitmask_blocks[i] = (ccos_bitmask_t*)header;
    } else {
      // previous block was the last bitnmask block
      result.length = i;
      break;
    }
  }

  return result;
}

// TODO: move to the separate file
uint16_t get_free_block(ccfs_handle ctx, const ccos_bitmask_list_t* bitmask_list) {
  size_t bitmask_size = get_bitmask_size(ctx);
  for (size_t block = 0; block < bitmask_list->length; block++) {
    for (int i = 0; i < bitmask_size; ++i) {
      uint8_t* bitmask_bytes = get_bitmask_bytes(bitmask_list->bitmask_blocks[block]);
      if (bitmask_bytes[i] != 0xFF) {
        uint8_t byte = bitmask_bytes[i];
        for (int j = 0; j < 8; ++j) {
          if (!(byte & 1u)) {
            return (block * bitmask_size * 8) + i * 8 + j;
          }

          byte = byte >> 1u;
        }
      }
    }
  }

  return CCOS_INVALID_BLOCK;
}

void mark_block(ccfs_handle ctx, ccos_bitmask_list_t* bitmask_list, uint16_t block, uint8_t mode) {
  TRACE("Mark block %x as %s...", block, mode ? "used" : "free");

  size_t bitmask_blocks = get_bitmask_blocks(ctx);

  if (block >= bitmask_list->length * bitmask_blocks) {
    fprintf(stderr, "Unable to mark block 0x%x: out of bitmask bounds of %lx!\n", block,
            bitmask_list->length * bitmask_blocks);
    return;
  }

  size_t bitmask_index = block / bitmask_blocks;
  block = block - (bitmask_index * bitmask_blocks);
  uint8_t* bytes = get_bitmask_bytes(bitmask_list->bitmask_blocks[bitmask_index]);
  uint8_t* byte = &bytes[block >> 3u];
  if (mode) {
    *byte = *byte | (1u << (block & 0b111u));
  } else {
    *byte = *byte & ~(1u << (block & 0b111u));
  }

  for (size_t i = 0; i < bitmask_list->length; i++) {
    if (mode) {
      // all bitmasks should have same "allocated" value (?)
      bitmask_list->bitmask_blocks[i]->allocated += 1;
    } else {
      bitmask_list->bitmask_blocks[i]->allocated -= 1;
    }

    update_bitmask_checksum(ctx, bitmask_list->bitmask_blocks[i]);
  }
}

ccos_inode_t* init_inode(ccfs_handle ctx, uint16_t block, uint16_t parent_dir_block, uint8_t* image_data) {
  TRACE("Initializing inode at 0x%x!", block);
  ccos_inode_t* inode = get_inode(ctx, block, image_data);
  memset(inode, 0, sizeof(ccos_inode_t));
  inode->header.file_id = block;
  inode->desc.dir_file_id = parent_dir_block;
  inode->content_inode_info.header.file_id = block;
  inode->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  inode->content_inode_info.block_current = block;
  inode->content_inode_info.block_prev = CCOS_INVALID_BLOCK;

  size_t inode_max_blocks = get_inode_max_blocks(ctx);
  uint16_t* content_blocks = get_inode_content_blocks(inode);

  for (int i = 0; i < inode_max_blocks; ++i) {
    content_blocks[i] = CCOS_CONTENT_BLOCKS_END_MARKER;
  }

  update_inode_checksums(ctx, inode);
  return inode;
}

ccos_content_inode_t* add_content_inode(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list) {
  ccos_block_data_t* content_inode_info = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = get_last_content_inode(ctx, file, data);
  if (last_content_inode != NULL) {
    content_inode_info = &(last_content_inode->content_inode_info);
  }

  uint16_t new_block = get_free_block(ctx, bitmask_list);
  if (new_block == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to allocate new content inode: No free space!\n");
    return NULL;
  }

  mark_block(ctx, bitmask_list, new_block, 1);

  ccos_content_inode_t* content_inode = get_content_inode(ctx, new_block, data);

  content_inode->content_inode_info.header.file_id = content_inode_info->header.file_id;
  content_inode->content_inode_info.header.file_fragment_index = content_inode_info->header.file_fragment_index;
  content_inode->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  content_inode->content_inode_info.block_current = new_block;
  content_inode->content_inode_info.block_prev = content_inode_info->block_current;

  content_inode_info->block_next = new_block;

  update_content_inode_checksums(ctx, content_inode);
  if (last_content_inode != NULL) {
    update_content_inode_checksums(ctx, last_content_inode);
  } else {
    update_inode_checksums(ctx, file);
  }

  return content_inode;
}

ccos_content_inode_t* get_last_content_inode(ccfs_handle ctx, const ccos_inode_t* file, const uint8_t* image_data) {
  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    ccos_content_inode_t* result = get_content_inode(ctx, file->content_inode_info.block_next, image_data);
    while (result->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
      result = get_content_inode(ctx, result->content_inode_info.block_next, image_data);
    }

    return result;
  }

  return NULL;
}

void erase_block(ccfs_handle ctx, uint16_t block, uint8_t* image, ccos_bitmask_list_t* bitmask_list) {
  size_t block_size = get_block_size(ctx);
  uint32_t address = block * block_size;
  memset(&image[address], 0, block_size);
  *(uint32_t*)&(image[address]) = CCOS_EMPTY_BLOCK_MARKER;
  mark_block(ctx, bitmask_list, block, 0);
}

int remove_content_inode(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list) {
  if (file->content_inode_info.block_next == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to remove content inode: no content inodes found in file %*s (0x%x)!\n",
            file->desc.name_length, file->desc.name, file->header.file_id);
    return -1;
  }

  ccos_content_inode_t* prev_inode = NULL;
  ccos_block_data_t* prev_block_data = &(file->content_inode_info);
  ccos_content_inode_t* last_content_inode = get_last_content_inode(ctx, file, data);

  erase_block(ctx, last_content_inode->content_inode_info.block_current, data, bitmask_list);

  prev_block_data->block_next = CCOS_INVALID_BLOCK;
  if (prev_inode != NULL) {
    update_content_inode_checksums(ctx, prev_inode);
  } else {
    update_inode_checksums(ctx, file);
  }

  return 0;
}

// remove last content block from the file
int remove_block_from_file(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list) {
  uint16_t* content_blocks = get_inode_content_blocks(file);
  ccos_content_inode_t* last_content_inode = get_last_content_inode(ctx, file, data);
  int content_blocks_count = get_inode_max_blocks(ctx);
  if (last_content_inode != NULL) {
    content_blocks = get_content_inode_content_blocks(last_content_inode);
    content_blocks_count = get_content_inode_max_blocks(ctx);
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
    erase_block(ctx, last_content_block, data, bitmask_list);
    *(uint16_t*)&(content_blocks[last_content_block_index - 1]) = CCOS_INVALID_BLOCK;
  }

  if (last_content_block_index <= 1) {
    if (remove_content_inode(ctx, file, data, bitmask_list) == -1) {
      fprintf(stderr, "Unable to remove content inode after freeing block at file 0x%x!\n", file->header.file_id);
      return -1;
    }
  }

  if (last_content_inode != NULL) {
    update_content_inode_checksums(ctx, last_content_inode);
  } else {
    update_inode_checksums(ctx, file);
  }

  return 0;
}

// get new block from empty blocks, modify it's header properly, reference it in the inode
uint16_t add_block_to_file(ccfs_handle ctx, ccos_inode_t* file, uint8_t* data, ccos_bitmask_list_t* bitmask_list) {
  ccos_content_inode_t* last_content_inode = NULL;

  size_t block_size = get_block_size(ctx);

  uint16_t* content_blocks = get_inode_content_blocks(file);
  size_t max_content_blocks = get_inode_max_blocks(ctx);

  int content_blocks_count = max_content_blocks;
  if (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    TRACE("Has content inode!");
    last_content_inode = get_last_content_inode(ctx, file, data);
    content_blocks = get_content_inode_content_blocks(last_content_inode);
    content_blocks_count = get_content_inode_max_blocks(ctx);
  }

  uint16_t last_content_block = 0;
  int last_content_block_index = 0;
  TRACE("%x (%*s): %d content blocks", file->header.file_id, file->desc.name_length, file->desc.name, content_blocks_count);
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

  uint16_t new_block = get_free_block(ctx, bitmask_list);
  if (new_block == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to allocate new content block: No free space!\n");
    return CCOS_INVALID_BLOCK;
  }

  TRACE("Allocating content block 0x%x for file id 0x%x.", new_block, file->header.file_id);
  mark_block(ctx, bitmask_list, new_block, 1);

  TRACE("Last content block is 0x%x", last_content_block);
  uint32_t new_block_address = new_block * block_size;
  ccos_block_header_t* new_block_header = (ccos_block_header_t*)&(data[new_block_address]);
  new_block_header->file_id = file->header.file_id;

  if (last_content_block != CCOS_INVALID_BLOCK) {
    uint32_t last_block_address = last_content_block * block_size;
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

    ccos_content_inode_t* new_content_inode = add_content_inode(ctx, file, data, bitmask_list);
    if (new_content_inode == NULL) {
      fprintf(stderr, "Unable to append new content inode to the file: no free space!\n");
      return CCOS_INVALID_BLOCK;
    }

    last_content_inode = new_content_inode;
    content_blocks = get_content_inode_content_blocks(new_content_inode);
    content_blocks_count = get_content_inode_max_blocks(ctx);
    last_content_block_index = 0;
  }

  // append new content block to the list; mark next block in the list as invalid; update checksum;
  content_blocks[last_content_block_index] = new_block;
  TRACE("Content block at %d is now 0x%x.", last_content_block_index, content_blocks[last_content_block_index]);
  if (last_content_block_index < content_blocks_count) {
    content_blocks[last_content_block_index + 1] = CCOS_INVALID_BLOCK;
  }

  update_inode_checksums(ctx, file);
  if (last_content_inode != NULL) {
    update_content_inode_checksums(ctx, last_content_inode);
  }

  return new_block;
}

int add_file_to_directory(ccfs_handle ctx, ccos_inode_t* directory, ccos_inode_t* file, uint8_t* image_data, size_t image_size) {
  if (add_file_entry_to_dir_contents(ctx, directory, image_data, image_size, file) == -1) {
    fprintf(stderr, "Unable to add file with id 0x%x to directory with id 0x%x!\n", file->header.file_id,
            directory->header.file_id);
    return -1;
  }

  file->desc.dir_file_id = directory->header.file_id;
  directory->desc.dir_count += 1;

  update_inode_checksums(ctx, file);
  update_inode_checksums(ctx, directory);

  return 0;
}

int parse_directory_data(ccfs_handle ctx, uint8_t* image_data,
                         const uint8_t* directory_data, size_t directory_data_size,
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
    (*entries)[count].file = get_inode(ctx, entry_block, image_data);

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
  uint8_t reverse_length = file->desc.name_length + sizeof(dir_entry_t) + sizeof(reverse_length);
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
  (*directory_entry)[offset] = file->desc.name_length;

  offset += sizeof(file->desc.name_length);
  memcpy(*directory_entry + offset, file->desc.name, file->desc.name_length);

  offset += file->desc.name_length;
  (*directory_entry)[offset] = reverse_length;

  offset += sizeof(reverse_length);
  (*directory_entry)[offset] = last_entry_flag;

  return 0;
}

// find a place for the new filename in dir contents (all files are located there in alphabetical, case-insensitive
// order), and insert it there
int add_file_entry_to_dir_contents(ccfs_handle ctx, ccos_inode_t* directory,
                                   uint8_t* image_data, size_t image_size,
                                   ccos_inode_t* file) {
  TRACE("Directory size: %d bytes, length: %d, has %d entries",
        directory->desc.file_size, directory->desc.dir_length,
        directory->desc.dir_count);

  uint8_t* directory_data = NULL;
  size_t dir_size = 0;
  if (ccos_read_file(ctx, directory, image_data, &directory_data, &dir_size) == -1) {
    fprintf(stderr, "Unable to get directory contents: Unable to read directory!\n");
    if (directory_data != NULL) {
      free(directory_data);
      return -1;
    }
  }

  parsed_directory_element_t* elements = NULL;
  if (parse_directory_data(ctx, image_data, directory_data, dir_size, directory->desc.dir_count, &elements) == -1) {
    fprintf(stderr, "Unable to add file to directory files list: Unable to parse directory data!\n");
    return -1;
  }

  // 1. Find place for the new file to insert
  int i = find_file_index_in_directory_data(file, directory, elements);
  if ((i < directory->desc.dir_count) && (file->desc.name_length == elements[i].file->desc.name_length) &&
      !(strncasecmp(file->desc.name, elements[i].file->desc.name, file->desc.name_length))) {
    // TODO: add option to overwrite existing file
    fprintf(stderr, "Unable to add file %*s to the directory: File exists!\n", file->desc.name_length, file->desc.name);
    free(directory_data);
    return -1;
  }

  int new_entry_is_last = i == directory->desc.dir_count;

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
  if (directory->desc.dir_count > 0) {
    real_dir_size = elements[directory->desc.dir_count - 1].offset + elements[directory->desc.dir_count - 1].size + 1;
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
    if (directory->desc.dir_count == 0) {
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
    if (directory->desc.dir_count == 0) {
      // Empty directory contains only last entry flag
      directory_data[0] = 0;
    } else {
      directory_data[elements[i - 1].offset + elements[i - 1].size] = 0;
    }
  }

  // 5. Save changes
  int res = ccos_write_file(ctx, directory, image_data, image_size, directory_data, new_dir_size);
  free(directory_data);
  if (res == -1) {
    fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", directory->header.file_id);
    return -1;
  }

  return 0;
}

int delete_file_from_parent_dir(ccfs_handle ctx, ccos_inode_t* file, uint8_t* image_data, size_t image_size) {
    ccos_inode_t* parent_dir = ccos_get_parent_dir(ctx, file, image_data);

    TRACE("Reading contents of the directory %*s (0x%x)",
          parent_dir->desc.name_length, parent_dir->desc.name,
          parent_dir->header.file_id);

    size_t dir_size = 0;
    uint8_t* directory_data = NULL;

    if (ccos_read_file(ctx, parent_dir, image_data, &directory_data, &dir_size) == -1) {
      fprintf(stderr, "Unable to read directory contents at directory id 0x%x\n", parent_dir->header.file_id);
      return -1;
    }

    parsed_directory_element_t* elements = NULL;
    if (parse_directory_data(ctx, image_data, directory_data, dir_size, parent_dir->desc.dir_count, &elements) == -1) {
      fprintf(stderr, "Unable to add file to directory files list: Unable to parse directory data!\n");
      free(directory_data);
      return -1;
    }

    // Find place of the file to delete in directory data
    int i = find_file_index_in_directory_data(file, parent_dir, elements);
    if ((i < parent_dir->desc.dir_count) && (file->desc.name_length == elements[i].file->desc.name_length) &&
        !(strncasecmp(file->desc.name, elements[i].file->desc.name, file->desc.name_length))) {
      TRACE("File is found!");
    } else {
      fprintf(stderr, "Unable to find file \"%*s\" in directory \"%*s\"!\n",
              file->desc.name_length, file->desc.name,
              parent_dir->desc.name_length, parent_dir->desc.name);
      free(directory_data);
      free(elements);
      return -1;
    }

    int entry_to_delete_is_last = i == (parent_dir->desc.dir_count - 1);

    // If we remove last entry, mark the one before it as last.
    if (entry_to_delete_is_last) {
      if (parent_dir->desc.dir_count == 1) {
        directory_data[0] = CCOS_DIR_LAST_ENTRY_MARKER;
      } else {
        directory_data[elements[i - 1].offset + elements[i - 1].size] = CCOS_DIR_LAST_ENTRY_MARKER;
      }
    }

    // |  <------------- elements[i].size -------------->  |            |
    // |  .-- elements[i].offset                           |            |  .-- elements[i+1].offset
    // |  V                                                |            |  V
    // |  00  01  |   02    | 03 04 05 ... NN |    NN+1    |    NN+2    |  +3  +4  |   +5    |
    // |----------|---------|-----------------|------------|------------|----------|---------|
    // | file id  |  name   |      name       |  reversed  | last entry | file id  |  name   |
    // |          | length  |                 |  length    |    flag    |          | length  |
    size_t next_entry_offset = elements[i].offset + elements[i].size + sizeof(uint8_t);

    if (parent_dir->desc.dir_count > 1) {
      memmove(directory_data + elements[i].offset, directory_data + next_entry_offset, dir_size - next_entry_offset);
    }

    // Zero last bytes at the end of dir contents. It's not necessary if you have last entry marker set correctly, but
    // it'll help read image in HEX editor if removed dir entry will be nice and zeroed.
    size_t shrink_size = elements[i].size + sizeof(uint8_t);
    memset(directory_data + dir_size - shrink_size, 0, shrink_size);
    size_t new_dir_size = dir_size - shrink_size;

    // Write dir contents back with old size to overwrite bytes at the end of dir with zeroes.
    int res = ccos_write_file(ctx, parent_dir, image_data, image_size, directory_data, dir_size);

    // Do that once more with new size to clear freed up content block.
    if (res != -1) {
      res = ccos_write_file(ctx, parent_dir, image_data, image_size, directory_data, new_dir_size);
    }

    free(directory_data);
    free(elements);

    if (res == -1) {
      fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", parent_dir->header.file_id);
      return -1;
    }

    parent_dir->desc.file_size = new_dir_size;
    parent_dir->desc.dir_length = new_dir_size;
    parent_dir->desc.dir_count -= 1;

    update_inode_checksums(ctx, parent_dir);

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
  for (i = 0; i < directory->desc.dir_count; ++i) {
    TRACE("Parsing entry # %d...", i);

    memset(entry_name, 0, CCOS_MAX_FILE_NAME);
    memset(entry_type, 0, CCOS_MAX_FILE_NAME);
    parse_file_name((const short_string_t*)&(elements[i].file->desc.name_length), entry_name, entry_type, &entry_name_length,
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

int get_block_data(ccfs_handle ctx, uint16_t block, const uint8_t* data, const uint8_t** start, size_t* size) {
  size_t block_size = get_block_size(ctx);
  size_t log_block_size = get_log_block_size(ctx);
  // TODO: check bounds
  uint32_t address = block * block_size;
  *start = &(data[address + CCOS_DATA_OFFSET]);
  *size = log_block_size;
  return 0;
}

int get_free_blocks(ccfs_handle ctx, ccos_bitmask_list_t* bitmask_list, size_t data_size, size_t* free_blocks_count,
                    uint16_t** free_blocks) {
  size_t free_count = 0;

  size_t block_size = get_block_size(ctx);
  size_t block_count = data_size / block_size;

  // sanity checks
  int allocated_info[MAX_BITMASK_BLOCKS_IN_IMAGE];
  for (size_t i = 0; i < bitmask_list->length; i++) {
    allocated_info[i] = bitmask_list->bitmask_blocks[i]->allocated;
    uint16_t checksum = calc_bitmask_checksum(ctx, bitmask_list->bitmask_blocks[i]);
    if (bitmask_list->bitmask_blocks[i]->checksum != checksum) {
      fprintf(stderr, "Warn: bitmask #" SIZE_T " checksum mismatch! Expected: 0x%x, got: 0x%x!\n", i,
              bitmask_list->bitmask_blocks[i]->checksum, checksum);
    }

    for (size_t j = 0; j < i; j++) {
      if (allocated_info[i] != allocated_info[j]) {
        fprintf(stderr, "Warn: bitmask blocks allocated value mismatch: #" SIZE_T " has %d; #" SIZE_T " has %d!\n", i,
                allocated_info[i], j, allocated_info[j]);
      }
    }
  }

  // We use first block to get allocated info
  TRACE("Allocated: %d, total: %d", bitmask_list->bitmask_blocks[0]->allocated, block_count);
  *free_blocks_count = block_count - bitmask_list->bitmask_blocks[0]->allocated;
  TRACE("Free blocks: %d", *free_blocks_count);
  *free_blocks = (uint16_t*)calloc(*free_blocks_count, sizeof(uint16_t));
  if (*free_blocks == NULL) {
    fprintf(stderr, "Unable to allocate " SIZE_T " bytes for free blocks: %s!\n", *free_blocks_count * sizeof(uint16_t),
            strerror(errno));
    return -1;
  }

  for (size_t block = 0; block < bitmask_list->length; block++) {
    for (int i = 0; i < get_bitmask_size(ctx); ++i) {
      uint8_t* bitmaks_bytes = get_bitmask_bytes(bitmask_list->bitmask_blocks[block]);
      if (bitmaks_bytes[i] != 0xFF) {
        uint8_t byte = bitmaks_bytes[i];
        for (int j = 0; j < 8; ++j) {
          if (!(byte & 1u)) {
            free_count++;
            if (free_count < *free_blocks_count) {
              (*free_blocks)[free_count] = (block * get_bitmask_blocks(ctx)) + i * 8 + j;
            }
          }
          byte = byte >> 1u;
        }
      }
    }
  }

  if (free_count != *free_blocks_count) {
    fprintf(stderr, "Warn: free block count (" SIZE_T ") mismatches found free blocks (" SIZE_T ")!\n",
            *free_blocks_count, free_count);
  }

  return 0;
}

int is_root_dir(const ccos_inode_t* file) {
  // In CCOS, root directory's parent file id it its own file id.
  return file->header.file_id == file->desc.dir_file_id;
}

int change_date(ccfs_handle ctx, ccos_inode_t* file, ccos_date_t new_date, date_type_t type) {
  if (is_root_dir(file)) return -1;

  if (type == CREATED) {
    file->desc.creation_date = new_date;
  } else if (type == MODIF) {
    file->desc.mod_date = new_date;
  } else if (type == EXPIR) {
    file->desc.expiration_date = new_date;
  } else {
    return -1;
  }
  update_inode_checksums(ctx, file);
  return 0;
}

int format_image(ccfs_handle ctx, uint8_t* data, size_t image_size) {
  size_t block_size = get_block_size(ctx);
  size_t bitmask_size = get_bitmask_size(ctx);

  uint16_t superblock = ctx->superblock_id;

  size_t sb_offset = block_size * superblock;
  if (sb_offset >= image_size) {
    fprintf(stderr, "Unable to format image: image_size < superblock offset!\n");
    return -1;
  }

  // Set superblock
  uint16_t* sb_offset_addr = ((uint16_t*)&(data[CCOS_SUPERBLOCK_ADDR_OFFSET]));
  *sb_offset_addr = superblock;

  uint16_t* sb_addr = ((uint16_t*)&(data[sb_offset]));
  *sb_addr = superblock;

  // Create bitmask
  size_t free_blocks = image_size / block_size;
  size_t free_bitmask_count = free_blocks / 8;
  // How many bitmask blocks we need for the given image size?
  size_t bitmask_blocks_count = (free_bitmask_count + bitmask_size - 1) / bitmask_size;
  // How many blocks we mark as free at the last bitmask block?
  size_t free_bitmask_remainder = bitmask_size - (bitmask_blocks_count * bitmask_size - free_bitmask_count);
  uint16_t bitmask_file_id = superblock - bitmask_blocks_count;

  uint16_t* bitmask_offset_addr = (uint16_t*)&(data[CCOS_BITMASK_ADDR_OFFSET]);
  *bitmask_offset_addr = bitmask_file_id;

  for (size_t i = bitmask_blocks_count; i > 0; i--) {
    uint16_t bitmask_block = superblock - i;
    ccos_bitmask_t* bitmask = (ccos_bitmask_t*)&(data[bitmask_block * block_size]);
    bitmask->header.file_id = bitmask_file_id;
    bitmask->header.file_fragment_index = bitmask_blocks_count - i;

    uint8_t* bitmask_bytes = get_bitmask_bytes(bitmask);
    if (i == 1) {
      // last block
      memset(bitmask_bytes, 0, free_bitmask_remainder);
      memset(bitmask_bytes + free_bitmask_remainder, 0xFF, bitmask_size - free_bitmask_remainder);
    } else {
      memset(bitmask_bytes, 0, bitmask_size);
    }
  }

  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(ctx, data, image_size);
  for (size_t j = bitmask_blocks_count; j > 0; j--) {
    mark_block(ctx, &bitmask_list, superblock - j, 1);  // mark bitmask blocks as used
  }
  mark_block(ctx, &bitmask_list, superblock, 1);      // superblock
  mark_block(ctx, &bitmask_list, superblock + 1, 1);  // superblock contents

  // Format root directory
  ccos_inode_t* root_dir = (ccos_inode_t*)sb_addr;
  root_dir->header.file_id = superblock;
  root_dir->header.file_fragment_index = 0;

  // Root directory is it's own parent
  root_dir->desc.dir_file_id = root_dir->header.file_id;
  root_dir->desc.file_size = get_dir_default_size(ctx);

  // Same as in GRiD-OS formatted blank image
  root_dir->desc.protec = 0x1;
  root_dir->desc.pswd_len = 0x5;
  root_dir->desc.pswd[0] = '\x29';
  root_dir->desc.pswd[1] = '\xFF';
  root_dir->desc.pswd[2] = '\x47';
  root_dir->desc.pswd[3] = '\xC7';

  // Fill root directory contents
  root_dir->content_inode_info.header.file_id = superblock;
  root_dir->content_inode_info.header.file_fragment_index = 0;
  root_dir->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  root_dir->content_inode_info.block_current = superblock;
  root_dir->content_inode_info.block_prev = CCOS_INVALID_BLOCK;

  // Fill content blocks
  uint16_t* content_blocks = get_inode_content_blocks(root_dir);
  size_t max_content_blocks = get_inode_max_blocks(ctx);

  memset(content_blocks, 0xFF, max_content_blocks);

  uint16_t superblock_entry_block = superblock + 1;
  content_blocks[0] = superblock_entry_block;

  update_inode_checksums(ctx, root_dir);

  // Root directory contents
  ccos_block_header_t* superblock_entry = (ccos_block_header_t*)get_inode(ctx, superblock_entry_block, data);
  superblock_entry->file_id = superblock;
  superblock_entry->file_fragment_index = 0;
  ((uint16_t*)superblock_entry)[2] = CCOS_DIR_LAST_ENTRY_MARKER;

  return 0;
}
