#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#include <stdint.h>

typedef struct {
  uint16_t sector_size;
  uint16_t superblock_id;
  uint16_t bitmap_block_id;
} ccfs_context_t;

typedef const ccfs_context_t* ccfs_handle;

#endif  // CCOS_CONTEXT_H
