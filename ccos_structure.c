#include "ccos_structure.h"

#define SECTOR_SIZE_TO_CONST(name, X) \
  size_t get_##name (ccfs_handle ctx) { \
    switch (ctx->sector_size) { \
      case SECTOR_SIZE_BUBBLE_MEMORY: return BUBMEM_##X;  \
      case SECTOR_SIZE_EXTERNAL_DISK: return EXTDISK_##X; \
      default: return SIZE_MAX; \
    } \
  }

SECTOR_SIZE_TO_CONST(block_size,                BLOCK_SIZE);
SECTOR_SIZE_TO_CONST(log_block_size,            LOG_BLOCK_SIZE);
SECTOR_SIZE_TO_CONST(inode_max_blocks,          INODE_MAX_BLOCKS);
SECTOR_SIZE_TO_CONST(content_inode_max_blocks,  CONTENT_MAX_BLOCKS);
SECTOR_SIZE_TO_CONST(bitmask_size,              BITMASK_SIZE);
SECTOR_SIZE_TO_CONST(bitmask_blocks,            BITMASK_BLOCKS);
SECTOR_SIZE_TO_CONST(dir_default_size,          DIR_DEFAULT_SIZE);

uint16_t* get_inode_content_blocks(ccfs_handle ctx, ccos_inode_t* inode) {
  switch (ctx->sector_size) {
    case SECTOR_SIZE_BUBBLE_MEMORY: return (uint16_t*)&inode->bs256.content_blocks;
    case SECTOR_SIZE_EXTERNAL_DISK: return (uint16_t*)&inode->bs512.content_blocks;
    default: return NULL;
  }
}

uint16_t* get_content_inode_content_blocks(ccfs_handle ctx, ccos_content_inode_t* inode) {
  switch (ctx->sector_size) {
    case SECTOR_SIZE_BUBBLE_MEMORY: return (uint16_t*)&inode->bs256.content_blocks;
    case SECTOR_SIZE_EXTERNAL_DISK: return (uint16_t*)&inode->bs512.content_blocks;
    default: return NULL;
  }
}

uint8_t* get_bitmask_bytes(ccfs_handle ctx, ccos_bitmask_t* bitmask) {
  switch (ctx->sector_size) {
    case SECTOR_SIZE_BUBBLE_MEMORY: return (uint8_t*)&bitmask->bs256.bytes;
    case SECTOR_SIZE_EXTERNAL_DISK: return (uint8_t*)&bitmask->bs512.bytes;
    default: return NULL;
  }
}
