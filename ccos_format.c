#include "ccos_format.h"
#include "ccos_format_data.h"

#include "common.h"
#include "ccos_context.h"
#include "ccos_structure.h"
#include "ccos_private.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

static uint8_t* new_empty_image(uint16_t sector_size, size_t bytes) {
  uint8_t* image = calloc(bytes, sizeof(uint8_t));
  if (image == NULL) {
    return NULL;
  }

  const size_t sector_count = bytes / sector_size;
  for (size_t i = 0; i < sector_count; i++) {
    uint8_t* sector = image + (i * sector_size);
    size_t marker_size = 4;

    memset(sector, 0xff, marker_size);
    memset(sector + marker_size, 0x55, sector_size - marker_size);
  }

  return image;
}

static ccos_bitmask_list_t init_bitmask(ccos_disk_t* disk) {
  const size_t sector_count = disk->size / disk->sector_size;
  const size_t bitmask_required_bytes = sector_count / 8;
  const size_t bitmask_bytes_per_sector = get_bitmask_size((ccfs_handle)disk);
  const size_t bitmask_sectors = bitmask_required_bytes / bitmask_bytes_per_sector + 1;

  const size_t bitmask_tail_length = bitmask_bytes_per_sector - (bitmask_required_bytes % bitmask_bytes_per_sector);
  const size_t bitmask_tail_offset = bitmask_required_bytes % bitmask_bytes_per_sector;

  // InitializeMedia~Run~ formats the bitmask based on values from the disk status.
  // Since we are creating the disk ourselves, decided to place the bitmask in the sectors
  // before the superblock. However, the bitmask can be placed anywhere..
  const size_t superblock_fid = disk->sector_size == 512
    ? DEFAULT_SUPERBLOCK
    : DEFAULT_BUBBLE_SUPERBLOCK;
  const size_t bitmask_fid = superblock_fid - bitmask_sectors;

  // Update disk structure with correct value.
  disk->bitmap_block_id = bitmask_fid;
  disk->superblock_id = bitmask_fid + bitmask_sectors;

  // Initialize empty bitmask.
  for (size_t i = 0; i < bitmask_sectors; i++) {
    const size_t sector_offset = (bitmask_fid + i) * disk->sector_size;
    ccos_bitmask_t* bitmask = (ccos_bitmask_t*)&disk->data[sector_offset];

    memset(bitmask, 0x00, disk->sector_size);
  
    bitmask->header.file_id = bitmask_fid;
    bitmask->header.file_fragment_index = i;
    bitmask->allocated = 0;
  
    if (i == bitmask_sectors - 1) {
      uint8_t* bitmask_bytes = get_bitmask_bytes(bitmask);
      memset(bitmask_bytes + bitmask_tail_offset, 0xff, bitmask_tail_length);
    }

    update_bitmask_checksum((ccfs_handle)disk, bitmask);
  }

  // Build list of bitmask blocks.
  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks((ccfs_handle)disk, disk->data, disk->size);

  // Mark the bitmask blocks as used in the bitmask itself.
  for (size_t i = 0; i < bitmask_sectors; i++) {
    mark_block((ccfs_handle)disk, &bitmask_list, bitmask_fid + i, 1);
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
  uint16_t id = disk->superblock_id;

  const size_t sector_offset = id * disk->sector_size;
  ccos_inode_t* root_dir = (ccos_inode_t*)&disk->data[sector_offset];

  memset(root_dir, 0x00, disk->sector_size);

  root_dir->header.file_id = id;
  root_dir->header.file_fragment_index = 0;

  root_dir->desc.file_size = get_dir_default_size((ccfs_handle)disk);

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

  mark_block((ccfs_handle)disk, bitmask_list, id, 1);

  uint16_t* content_blocks = get_inode_content_blocks(root_dir);
  size_t max_content_blocks = get_inode_max_blocks((ccfs_handle)disk);

  memset(content_blocks, 0xFF, max_content_blocks * sizeof(uint16_t));

  uint16_t superblock_entry_block = id + 1;
  content_blocks[0] = superblock_entry_block;

  update_inode_checksums((ccfs_handle)disk, root_dir);

  ccos_block_header_t* superblock_entry = (ccos_block_header_t*)get_inode((ccfs_handle)disk, superblock_entry_block, disk->data);
  memset(superblock_entry, 0x00, disk->sector_size);
  superblock_entry->file_id = id;
  superblock_entry->file_fragment_index = 0;
  ((uint16_t*)superblock_entry)[2] = CCOS_DIR_LAST_ENTRY_MARKER;

  mark_block((ccfs_handle)disk, bitmask_list, superblock_entry_block, 1);
}

static void write_boot_code(ccos_disk_t* disk, ccos_bitmask_list_t* bitmask_list) {
  // TODO: Support GRiDCase.
  const uint8_t* boot_code = COMPASS_BOOT_CODE;

  size_t pages = sizeof(COMPASS_BOOT_CODE) / disk->sector_size;
  size_t offset = sizeof(ZERO_PAGE) / disk->sector_size;

  for (size_t i = 0; i < pages; i++) {
    memcpy(disk->data + (offset + i) * disk->sector_size, COMPASS_BOOT_CODE + i * disk->sector_size, disk->sector_size);
    mark_block((ccfs_handle)disk, bitmask_list, offset + i, 1);
  }
}

static void write_zero_page(ccos_disk_t* disk, ccos_bitmask_list_t* bitmask_list) {
  size_t pages = sizeof(ZERO_PAGE) / disk->sector_size;
  for (size_t i = 0; i < pages; i++) {
    memcpy(disk->data + i * disk->sector_size, ZERO_PAGE + i * disk->sector_size, disk->sector_size);
    mark_block((ccfs_handle)disk, bitmask_list, i, 1);
  }
}

static void write_blocks_numbers(ccos_disk_t* disk) {
  // InitializeMedia~Run~ does not initialize these fields because it thinks the superblock
  // will be located by the disk status.
  // 
  // In our case, there is no real disk, so the sector numbers must be saved in the image itself.
  *(uint16_t*)(disk->data + CCOS_SUPERBLOCK_ADDR_OFFSET) = disk->superblock_id;
  *(uint16_t*)(disk->data + CCOS_BITMASK_ADDR_OFFSET) = disk->bitmap_block_id;
}

int ccos_new_disk_image(uint16_t sector_size, size_t bytes, ccos_disk_t* output) {
  if (sector_size != 256 && sector_size != 512) {
    TRACE("Format image: invalid sector size");
    return EINVAL;
  }

  if (bytes % sector_size != 0) {
    TRACE("Format image: image size %zu is not a multiple of sector size %zu", bytes, sector_size);
    return EINVAL;
  }

  uint8_t* data = new_empty_image(sector_size, bytes);
  if (data == NULL) {
    return ENOMEM;
  }

  ccos_disk_t disk = {
    .sector_size = sector_size,
    .superblock_id = 0,
    .bitmap_block_id = 0,
    .size = bytes,
    .data = data
  };

  ccos_bitmask_list_t bitmask_list = init_bitmask(&disk);
  write_superblock(&disk, &bitmask_list);

  write_boot_code(&disk, &bitmask_list);

  write_zero_page(&disk, &bitmask_list);
  write_blocks_numbers(&disk);

  *output = disk;

  return 0;
}
