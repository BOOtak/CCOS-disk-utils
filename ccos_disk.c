#include "ccos_disk.h"

void* ccos_disk_read(ccos_disk_t* disk, uint16_t sector) {
  if (disk == NULL || disk->data == NULL || disk->sector_size == 0) {
    return NULL;
  }

  size_t offset = (size_t)sector * (size_t)disk->sector_size;
  if (offset + disk->sector_size > disk->size) {
    return NULL;
  }

  return &disk->data[offset];
}
