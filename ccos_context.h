#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#include <stdint.h>

typedef enum {
  SECTOR_SIZE_BUBBLE_MEMORY = 256,
  SECTOR_SIZE_EXTERNAL_DISK = 512,
} ccos_sector_size_t;

typedef struct {
  ccos_sector_size_t sector_size;
  uint16_t superblock_id;
  uint16_t bitmap_block_id;
} ccfs_context_t;

typedef const ccfs_context_t* ccfs_handle;

#endif  // CCOS_CONTEXT_H
