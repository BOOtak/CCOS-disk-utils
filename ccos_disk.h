#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint16_t sector_size;
  uint16_t superblock_fid;
  uint16_t bitmap_fid;
  size_t   size;
  uint8_t* data;
} ccos_disk_t;

#endif  // CCOS_CONTEXT_H
