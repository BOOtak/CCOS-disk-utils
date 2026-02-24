#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint16_t sector_size;
  uint16_t superblock_id;
  uint16_t bitmap_block_id;
} ccfs_context_t;

typedef const ccfs_context_t* ccfs_handle;

typedef struct {
  uint16_t sector_size;
  uint16_t superblock_id;
  uint16_t bitmap_block_id;
  size_t   size;
  uint8_t* data;
} ccos_disk_t;

#endif  // CCOS_CONTEXT_H
