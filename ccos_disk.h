#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct {
  uint16_t sector_size;
  uint16_t superblock_fid;
  uint16_t bitmap_fid;
  size_t   size;
  uint8_t* data;
} ccos_disk_t;

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} ccos_version_t;

#ifdef __cplusplus
}
#endif

#endif  // CCOS_CONTEXT_H
