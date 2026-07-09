#include "ccos_private.h"

#include "ccos_structure.h"

#include <stdlib.h>

typedef enum {
  CCOS_DISK_SECTOR_FORMAT_256,
  CCOS_DISK_SECTOR_FORMAT_512,
} ccos_disk_sector_format_t;

struct ccos_disk_t_ {
  ccos_disk_sector_format_t sector_format;
  uint16_t superblock_fid;
  uint16_t bitmap_fid;
  size_t   size;
  uint8_t* data;
};

static uint16_t ccos_disk_sector_format_size(ccos_disk_sector_format_t sector_format) {
    if (sector_format == CCOS_DISK_SECTOR_FORMAT_256) {
        return BUBBLES_SECTOR_SIZE;
    } else {
        return EXTDISK_SECTOR_SIZE;
    }
}

static ccos_disk_t* ccos_disk_new(ccos_disk_sector_format_t sector_format, uint8_t* data, size_t size,
                                  uint16_t superblock, uint16_t bitmap) {
  uint16_t sector_size = ccos_disk_sector_format_size(sector_format);
  if (data == NULL || size == 0 || sector_size == 0 || size % sector_size != 0) {
    return NULL;
  }

  size_t sector_count = size / sector_size;
  if (superblock == 0 || bitmap == 0 || superblock == bitmap ||
      superblock >= sector_count || bitmap >= sector_count) {
    return NULL;
  }

  ccos_disk_t* disk = calloc(1, sizeof(ccos_disk_t));
  if (disk == NULL) {
    return NULL;
  }

  disk->sector_format = sector_format;
  disk->superblock_fid = superblock;
  disk->bitmap_fid = bitmap;
  disk->size = size;
  disk->data = data;

  return disk;
}

ccos_disk_t* ccos_disk_new_bubble(uint8_t* data, size_t size, uint16_t superblock, uint16_t bitmap) {
  return ccos_disk_new(CCOS_DISK_SECTOR_FORMAT_256, data, size, superblock, bitmap);
}

ccos_disk_t* ccos_disk_new_extdisk(uint8_t* data, size_t size, uint16_t superblock, uint16_t bitmap) {
  return ccos_disk_new(CCOS_DISK_SECTOR_FORMAT_512, data, size, superblock, bitmap);
}

void ccos_disk_free(ccos_disk_t* disk) {
  if (disk == NULL) {
    return;
  }

  free(disk->data);
  free(disk);
}

uint8_t* ccos_disk_data(ccos_disk_t* disk) {
  return disk == NULL ? NULL : disk->data;
}

size_t ccos_disk_size(const ccos_disk_t* disk) {
  return disk == NULL ? 0 : disk->size;
}

uint16_t ccos_disk_sector_size(const ccos_disk_t* disk) {
  return disk == NULL ? 0 : ccos_disk_sector_format_size(disk->sector_format);
}

uint16_t ccos_disk_superblock(const ccos_disk_t* disk) {
  return disk == NULL ? 0 : disk->superblock_fid;
}

uint16_t ccos_disk_bitmap(const ccos_disk_t* disk) {
  return disk == NULL ? 0 : disk->bitmap_fid;
}

void* ccos_disk_read(ccos_disk_t* disk, uint16_t sector) {
  uint16_t sector_size = ccos_disk_sector_size(disk);
  if (disk == NULL || disk->data == NULL || sector_size == 0) {
    return NULL;
  }

  size_t offset = (size_t)sector * (size_t)sector_size;
  if (offset + sector_size > ccos_disk_size(disk)) {
    return NULL;
  }

  return &disk->data[offset];
}
