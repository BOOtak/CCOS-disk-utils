#include "ccos_format.h"
#include "ccos_boot_data.h"

#include "ccos_boot_data.h"
#include "common.h"
#include "ccos_disk.h"
#include "ccos_structure.h"
#include "ccos_private.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define SECTOR(disk_ptr, id) ((void*)(disk_ptr)->data + (id) * (disk_ptr)->sector_size)

typedef struct {
  uint16_t sector;
  uint16_t count;
  uint16_t tail_length;
  uint16_t tail_offset;
} bitmask_info_t;

static uint8_t* new_empty_image(uint16_t sector_size, size_t disk_size) {
  uint8_t* image = calloc(disk_size, sizeof(uint8_t));
  if (image == NULL) {
    return NULL;
  }

  const size_t sector_count = disk_size / sector_size;
  for (size_t i = 0; i < sector_count; i++) {
    uint8_t* sector = image + (i * sector_size);
    size_t marker_size = 4;

    memset(sector, 0xff, marker_size);
    memset(sector + marker_size, 0x55, sector_size - marker_size);
  }

  return image;
}

static uint16_t select_superblock(uint16_t sector_size, size_t disk_size) {
  assert(sector_size == 256 || sector_size == 512);

  if (sector_size == 256) {
    return DEFAULT_BUBBLE_SUPERBLOCK;
  } else if (disk_size < 10 * 1024 * 1024) {
    return DEFAULT_SUPERBLOCK;
  } else {
    return DEFAULT_HDD_SUPERBLOCK;
  }
}

static bitmask_info_t calculate_bitmask_info(uint16_t sector_size, size_t disk_size) {
  assert(sector_size == 256 || sector_size == 512);

  const uint16_t superblock = select_superblock(sector_size, disk_size);

  // Calculate bitmask.
  const uint16_t sector_count = disk_size / sector_size;
  const uint16_t required_bytes = sector_count / 8;
  const uint16_t bytes_per_sector = sector_size == 512 ? BS512_BITMASK_SIZE : BS256_BITMASK_SIZE;
  const uint16_t count = required_bytes / bytes_per_sector + 1;

  const uint16_t bitmask = superblock - count;

  const uint16_t tail_length = bytes_per_sector - (required_bytes % bytes_per_sector);
  const uint16_t tail_offset = required_bytes % bytes_per_sector;

  // Combine and return.
  return (bitmask_info_t) {
    .sector = bitmask,
    .count = count,
    .tail_length = tail_length,
    .tail_offset = tail_offset,
  };
}

static ccos_bitmask_list_t init_bitmask(ccos_disk_t* disk, bitmask_info_t info) {
  // Initialize empty bitmask.
  for (size_t i = 0; i < info.count; i++) {
    ccos_bitmask_t* bitmask = (ccos_bitmask_t*)SECTOR(disk, info.sector + i);

    memset(bitmask, 0x00, disk->sector_size);
  
    bitmask->header.file_id = info.sector;
    bitmask->header.file_fragment_index = i;
    bitmask->allocated = 0;
  
    if (i == info.count - 1) {
      uint8_t* bitmask_bytes = get_bitmask_bytes(bitmask);
      memset(bitmask_bytes + info.tail_offset, 0xff, info.tail_length);
    }

    update_bitmask_checksum(disk, bitmask);
  }

  // Build list of bitmask blocks.
  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(disk);

  // Mark the bitmask blocks as used in the bitmask itself.
  for (size_t i = 0; i < info.count; i++) {
    mark_block(disk, &bitmask_list, info.sector + i, 1);
  }

  return bitmask_list;
}

static ccos_date_t get_current_date() {
  struct timespec tp;
  clock_gettime(CLOCK_REALTIME, &tp);
  struct tm* time_struct;
  time_struct = localtime(&tp.tv_sec);

  return (ccos_date_t) {
    .year = time_struct->tm_year + 1900,
    .month = time_struct->tm_mon + 1,
    .day = time_struct->tm_mday,
    .hour = time_struct->tm_hour,
    .minute =  time_struct->tm_min,
    .second = time_struct->tm_sec,
    .tenthOfSec = tp.tv_nsec / 100000000,
    .dayOfWeek = time_struct->tm_wday + 1,
    .dayOfYear = time_struct->tm_yday + 1};      
}

static void write_superblock(ccos_disk_t* disk, ccos_bitmask_list_t* bitmask_list) {
  uint16_t id = disk->superblock_fid;

  ccos_inode_t* root_dir = (ccos_inode_t*)SECTOR(disk, id);

  memset(root_dir, 0x00, disk->sector_size);

  root_dir->header.file_id = id;
  root_dir->header.file_fragment_index = 0;

  root_dir->desc.file_size = get_dir_default_size(disk);

  root_dir->desc.name_length = 0;
  memset(root_dir->desc.name, ' ', sizeof(root_dir->desc.name));

  root_dir->desc.creation_date = get_current_date();
  root_dir->desc.mod_date = root_dir->desc.creation_date;
  root_dir->desc.expiration_date = (ccos_date_t) {};

  root_dir->desc.dir_file_id = root_dir->header.file_id;

  root_dir->desc.protec = 1;
  root_dir->desc.pswd_len = 5;
  root_dir->desc.pswd[0] = '\x29';
  root_dir->desc.pswd[1] = '\xFF';
  root_dir->desc.pswd[2] = '\x47';
  root_dir->desc.pswd[3] = '\xC7';

  root_dir->content_inode_info.header.file_id = id;
  root_dir->content_inode_info.header.file_fragment_index = 0;
  root_dir->content_inode_info.block_next = CCOS_INVALID_BLOCK;
  root_dir->content_inode_info.block_current = id;
  root_dir->content_inode_info.block_prev = CCOS_INVALID_BLOCK;

  mark_block(disk, bitmask_list, id, 1);

  uint16_t* content_blocks = get_inode_content_blocks(root_dir);
  size_t max_content_blocks = get_inode_max_blocks(disk);

  memset(content_blocks, 0xFF, max_content_blocks * sizeof(uint16_t));

  uint16_t superblock_entry_block = id + 1;
  content_blocks[0] = superblock_entry_block;

  update_inode_checksums(disk, root_dir);

  ccos_block_header_t* superblock_entry = (ccos_block_header_t*)SECTOR(disk, superblock_entry_block);
  memset(superblock_entry, 0x00, disk->sector_size);
  superblock_entry->file_id = id;
  superblock_entry->file_fragment_index = 0;
  ((uint16_t*)superblock_entry)[2] = CCOS_DIR_LAST_ENTRY_MARKER;

  mark_block(disk, bitmask_list, superblock_entry_block, 1);
}

static void write_boot_code(ccos_disk_t* disk, disk_format_t format, ccos_bitmask_list_t* bitmask_list) {
  const uint8_t* boot_code = format == CCOS_DISK_FORMAT_GRIDCASE ? GRIDCASE_BOOT_CODE : COMPASS_BOOT_CODE;

  size_t pages = BOOT_CODE_SIZE / disk->sector_size;
  size_t offset = sizeof(ccos_boot_sector_t) / disk->sector_size;

  for (size_t i = 0; i < pages; i++) {
    memcpy(SECTOR(disk, offset + i), boot_code + i * disk->sector_size, disk->sector_size);
    mark_block(disk, bitmask_list, offset + i, 1);
  }
}

static void write_boot_sector(ccos_disk_t* disk, disk_format_t format, ccos_bitmask_list_t* bitmask_list) {
  ccos_boot_sector_t boot_sector = (ccos_boot_sector_t) {
    .superblock_fid = disk->superblock_fid,
    .bitmap_fid = disk->bitmap_fid,
  };

  if (format == CCOS_DISK_FORMAT_GRIDCASE) {
    memcpy(boot_sector.header, GRIDCASE_BOOT_SECTOR_HEADER, BOOT_SECTOR_HEADER_SIZE);
    memcpy(boot_sector.boot_code, GRIDCASE_BOOT_SECTOR_CODE, BOOT_SECTOR_CODE_SIZE);
  } else {
    memcpy(boot_sector.header, COMPASS_BOOT_SECTOR_HEADER, BOOT_SECTOR_HEADER_SIZE);
  }

  size_t pages = sizeof(ccos_boot_sector_t) / disk->sector_size;
  for (size_t i = 0; i < pages; i++) {
    memcpy(SECTOR(disk, i), &boot_sector + i * disk->sector_size, disk->sector_size);
    mark_block(disk, bitmask_list, i, 1);
  }
}

int ccos_new_disk_image(disk_format_t format, size_t disk_size, ccos_disk_t* output) {
  if (disk_size % 512 != 0) {
    TRACE("Format image: image size %zu is not a multiple of 512", disk_size);
    return EINVAL;
  }

  uint16_t sector_size = format == CCOS_DISK_FORMAT_BUBMEM ? 256 : 512;

  uint8_t* data = new_empty_image(sector_size, disk_size);
  if (data == NULL) {
    return ENOMEM;
  }

  // InitializeMedia~Run~ gets the block numbers from the disk status.
  // In the 2101 and 2102 firmwares, the bitmask and superblock numbers are hardcoded.
  // Because we create disk images ourselves, we can choose any values and save them inside the image.
  const uint16_t superblock = select_superblock(sector_size, disk_size);
  const bitmask_info_t bitmask = calculate_bitmask_info(sector_size, disk_size);

  ccos_disk_t disk = {
    .sector_size = sector_size,
    .superblock_fid = superblock,
    .bitmap_fid = bitmask.sector,
    .size = disk_size,
    .data = data
  };

  ccos_bitmask_list_t bitmask_list = init_bitmask(&disk, bitmask);
  write_superblock(&disk, &bitmask_list);

  write_boot_sector(&disk, format, &bitmask_list);
  write_boot_code(&disk, format, &bitmask_list);

  *output = disk;

  return 0;
}
