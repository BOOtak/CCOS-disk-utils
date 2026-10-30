#include "ccos_private.h"

#define SECTOR_SIZE_TO_CONST(name, X) \
  size_t ccos_get_##name (ccos_disk_t* disk) { \
    switch (ccos_disk_sector_size(disk)) {    \
      case 256: return BS256_##X;  \
      case 512: return BS512_##X;  \
      default:  return SIZE_MAX;   \
    } \
  }

SECTOR_SIZE_TO_CONST(log_sector_size,             LOG_BLOCK_SIZE);
SECTOR_SIZE_TO_CONST(inode_max_sectors,           INODE_MAX_BLOCKS);
SECTOR_SIZE_TO_CONST(content_inode_padding,       CONTENT_INODE_PADDING);
SECTOR_SIZE_TO_CONST(content_inode_max_sectors,   CONTENT_INODE_MAX_BLOCKS);
SECTOR_SIZE_TO_CONST(bitmask_size,                BITMASK_SIZE);
SECTOR_SIZE_TO_CONST(bitmask_sectors,             BITMASK_BLOCKS);
SECTOR_SIZE_TO_CONST(max_bitmask_blocks,          MAX_BITMASK_BLOCKS);
SECTOR_SIZE_TO_CONST(dir_default_size,            DIR_DEFAULT_SIZE);

uint16_t* ccos_get_inode_content_sectors(ccos_inode_t* inode) {
  return (uint16_t*)&inode->content;
}

uint16_t* ccos_get_content_inode_content_sectors(ccos_content_inode_t* inode) {
  return (uint16_t*)&inode->content;
}

uint8_t* ccos_get_bitmask_bytes(ccos_bitmask_t* bitmask) {
  return (uint8_t*)&bitmask->content;
}

ccos_bitmask_t* const* ccos_get_bitmask_blocks(ccos_disk_t* disk, const ccos_bitmask_list_t* bitmask_list) {
  switch (ccos_disk_sector_size(disk)) {
    case 256: return bitmask_list->content.bs256.blocks;
    case 512: return bitmask_list->content.bs512.blocks;
    default:  return NULL;
  }
}
