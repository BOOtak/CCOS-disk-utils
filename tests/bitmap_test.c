#include <criterion/criterion.h>
#include <criterion/redirect.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccos_format.h"
#include "ccos_image.h"
#include "ccos_private.h"

static uint8_t* create_test_data(size_t size) {
  uint8_t* data = malloc(size);
  if (data == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < size; ++i) {
    data[i] = (uint8_t)((i * 17u + i / 11u + 0x33u) & 0xFFu);
  }

  return data;
}

static ccos_inode_t* create_subject(ccos_disk_t* disk) {
  ccos_inode_t* root = ccos_get_root_dir(disk);
  cr_assert_not_null(root);

  ccos_inode_t* subject = ccos_create_dir(disk, root, "BitmapTest");
  cr_assert_not_null(subject, "ccos_create_dir failed");

  return subject;
}

static void assert_valid_bitmaps(ccos_disk_t* disk) {
  cr_assert(ccos_validate_disk_bitmap(disk));

  size_t free_space = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space), CCOS_OK);
}

static void assert_block_erased(ccos_disk_t* disk, uint16_t block) {
  uint32_t* block_data = ccos_disk_read(disk, block);
  cr_assert_not_null(block_data);
  cr_assert_eq(*block_data, CCOS_EMPTY_BLOCK_MARKER);
}

static size_t copy_bitmap_bytes(ccos_disk_t* disk, uint8_t** snapshot) {
  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t bitmask_size = ccos_get_bitmask_size(disk);
  size_t snapshot_size = bitmask_list.length * bitmask_size;
  *snapshot = malloc(snapshot_size);
  cr_assert_not_null(*snapshot);

  for (size_t i = 0; i < bitmask_list.length; ++i) {
    memcpy(*snapshot + i * bitmask_size, ccos_get_bitmask_bytes(bitmask_list.bitmask_blocks[i]), bitmask_size);
  }

  return snapshot_size;
}

static void assert_bitmap_bytes_equal(ccos_disk_t* disk, const uint8_t* expected, size_t expected_size) {
  uint8_t* actual = NULL;
  size_t actual_size = copy_bitmap_bytes(disk, &actual);

  cr_assert_eq(actual_size, expected_size);
  cr_assert_eq(memcmp(actual, expected, expected_size), 0);

  free(actual);
}

static void clear_first_tail_bit(ccos_disk_t* disk) {
  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t block_count = ccos_disk_size(disk) / ccos_disk_sector_size(disk);
  size_t blocks_per_bitmask = ccos_get_bitmask_sectors(disk);
  cr_assert_lt(block_count, bitmask_list.length * blocks_per_bitmask);

  size_t bitmask_index = block_count / blocks_per_bitmask;
  cr_assert_lt(bitmask_index, bitmask_list.length);

  size_t local_block = block_count - bitmask_index * blocks_per_bitmask;
  uint8_t* bytes = ccos_get_bitmask_bytes(bitmask_list.bitmask_blocks[bitmask_index]);
  bytes[local_block / 8] &= ~(1u << (local_block % 8));

  ccos_update_bitmask_checksum(disk, bitmask_list.bitmask_blocks[bitmask_index]);
}

static void assert_free_blocks_count_is_valid(ccos_disk_t* disk) {
  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t free_blocks_count = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks_count), CCOS_OK);

  size_t free_space = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space), CCOS_OK);
  cr_assert_eq(free_space, free_blocks_count * ccos_disk_sector_size(disk));

  size_t block_count = ccos_disk_size(disk) / ccos_disk_sector_size(disk);

  if (free_blocks_count > 0) {
    uint16_t free_block = ccos_get_free_sector(disk, &bitmask_list);
    cr_assert_lt(free_block, block_count);
  } else {
    cr_assert_eq(ccos_get_free_sector(disk, &bitmask_list), CCOS_INVALID_BLOCK);
  }
}

static void mark_all_real_blocks_used(ccos_disk_t* disk) {
  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  uint16_t free_block = ccos_get_free_sector(disk, &bitmask_list);
  while (free_block != CCOS_INVALID_BLOCK) {
    ccos_mark_sector(disk, &bitmask_list, free_block, 1);
    free_block = ccos_get_free_sector(disk, &bitmask_list);
  }
}

static void assert_fresh_image(disk_format_t format, size_t disk_size, uint16_t expected_sector_size) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(format, disk_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");
  cr_assert_eq(ccos_disk_sector_size(disk), expected_sector_size);
  cr_assert_eq((ccos_disk_size(disk) / ccos_disk_sector_size(disk)) % 8, 0);

  assert_valid_bitmaps(disk);
  assert_free_blocks_count_is_valid(disk);

  ccos_disk_free(disk);
}

static void assert_tail_free_bit_is_not_returned(disk_format_t format, size_t disk_size) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(format, disk_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");
  cr_assert_eq((ccos_disk_size(disk) / ccos_disk_sector_size(disk)) % 8, 0);

  mark_all_real_blocks_used(disk);
  clear_first_tail_bit(disk);

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_eq(ccos_get_free_sector(disk, &bitmask_list), CCOS_INVALID_BLOCK);

  size_t free_blocks_count = 1;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks_count), CCOS_OK);
  cr_assert_eq(free_blocks_count, 0);

  size_t free_space = 1;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space), CCOS_OK);
  cr_assert_eq(free_space, 0);

  ccos_disk_free(disk);
}

Test(bitmap, fresh_compass) {
  assert_fresh_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, 512);
}

Test(bitmap, fresh_512) {
  assert_fresh_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, 512);
}

Test(bitmap, fresh_bubbles) {
  assert_fresh_image(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024, 256);
}

Test(bitmap, tail_bits_512) {
  assert_tail_free_bit_is_not_returned(CCOS_DISK_FORMAT_COMPASS, 720 * 512);
}

Test(bitmap, tail_bits_256) {
  assert_tail_free_bit_is_not_returned(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}

Test(bitmap, reject_bad_allocated) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t block_count = ccos_disk_size(disk) / ccos_disk_sector_size(disk);
  bitmask_list.bitmask_blocks[0]->allocated = block_count + 1;
  ccos_update_bitmask_checksum(disk, bitmask_list.bitmask_blocks[0]);

  size_t free_blocks_count = 123;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks_count), CCOS_EINVAL);
  cr_assert_eq(free_blocks_count, 123);

  ccos_disk_free(disk);
}

Test(bitmap, count_uses_bits, .init = cr_redirect_stderr) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t expected_free_count = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &expected_free_count), CCOS_OK);

  bitmask_list.bitmask_blocks[0]->allocated = 0;
  ccos_update_bitmask_checksum(disk, bitmask_list.bitmask_blocks[0]);

  cr_assert_lt(expected_free_count, ccos_disk_size(disk) / ccos_disk_sector_size(disk));

  size_t free_blocks_count = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks_count), CCOS_OK);
  cr_assert_eq(free_blocks_count, expected_free_count);
  cr_assert_stderr_eq_str("");

  ccos_disk_free(disk);
}

Test(bitmap, validate_ok, .init = cr_redirect_stderr) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  cr_assert(ccos_validate_disk_bitmap(disk));
  cr_assert_stderr_eq_str("");

  ccos_disk_free(disk);
}

Test(bitmap, validate_allocated_count, .init = cr_redirect_stderr) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t free_count = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_count), CCOS_OK);

  bitmask_list.bitmask_blocks[0]->allocated = 0;
  ccos_update_bitmask_checksum(disk, bitmask_list.bitmask_blocks[0]);

  size_t block_count = ccos_disk_size(disk) / ccos_disk_sector_size(disk);
  char expected_warning[128];
  snprintf(expected_warning, sizeof(expected_warning),
           "Warn: free block count (" SIZE_T ") mismatches found free blocks (" SIZE_T ")!\n",
           block_count, free_count);

  cr_assert_not(ccos_validate_disk_bitmap(disk));
  cr_assert_stderr_eq_str(expected_warning);

  ccos_disk_free(disk);
}

Test(bitmap, validate_checksum, .init = cr_redirect_stderr) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 720 * 512, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  cr_assert_gt(bitmask_list.length, 0);

  ccos_bitmask_t* bitmask = bitmask_list.bitmask_blocks[0];
  uint16_t stored_checksum = bitmask->checksum ^ 1u;
  uint16_t actual_checksum = bitmask->checksum;
  bitmask->checksum = stored_checksum;

  char expected_warning[128];
  snprintf(expected_warning, sizeof(expected_warning),
           "Warn: bitmask #0 checksum mismatch! Expected: 0x%x, got: 0x%x!\n",
           stored_checksum, actual_checksum);

  cr_assert_not(ccos_validate_disk_bitmap(disk));
  cr_assert_stderr_eq_str(expected_warning);

  ccos_disk_free(disk);
}

Test(bitmap, free_space_tail_bits, .init = cr_redirect_stderr) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);

  clear_first_tail_bit(disk);

  size_t free_space_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after), CCOS_OK);
  cr_assert_eq(free_space_after, free_space_before);
  cr_assert_stderr_eq_str("");

  ccos_disk_free(disk);
}

Test(bitmap, delete_restores_bitmap) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  assert_valid_bitmaps(disk);

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);

  uint8_t file_data[] = {0x01, 0x23, 0x45, 0x67};
  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, sizeof(file_data), "Tiny~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");

  size_t data_blocks_count = 0;
  uint16_t* data_blocks = NULL;
  cr_assert_eq(ccos_get_file_sectors(disk, file, &data_blocks_count, &data_blocks), CCOS_OK);
  cr_assert_eq(data_blocks_count, 1);

  uint16_t inode_block = file->header.file_id;
  uint16_t data_block = data_blocks[0];

  assert_valid_bitmaps(disk);

  size_t free_space_after_add = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after_add), CCOS_OK);
  cr_assert_lt(free_space_after_add, free_space_before);

  cr_assert_eq(ccos_delete_file(disk, file), CCOS_OK);

  assert_block_erased(disk, inode_block);
  assert_block_erased(disk, data_block);
  assert_valid_bitmaps(disk);

  size_t free_space_after_delete = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after_delete), CCOS_OK);
  cr_assert_eq(free_space_after_delete, free_space_before);

  free(data_blocks);
  ccos_disk_free(disk);
}

Test(bitmap, delete_large_file) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  assert_valid_bitmaps(disk);

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);

  const size_t file_size = 1024 * 1024;
  uint8_t* file_data = create_test_data(file_size);
  cr_assert_not_null(file_data);

  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, file_size, "Huge~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");

  size_t data_blocks_count = 0;
  uint16_t* data_blocks = NULL;
  cr_assert_eq(ccos_get_file_sectors(disk, file, &data_blocks_count, &data_blocks), CCOS_OK);
  cr_assert_gt(data_blocks_count, ccos_get_inode_max_sectors(disk));

  cr_assert_neq(file->content_inode_info.block_next, CCOS_INVALID_BLOCK);

  uint16_t inode_block = file->header.file_id;
  assert_valid_bitmaps(disk);

  cr_assert_eq(ccos_delete_file(disk, file), CCOS_OK);

  assert_block_erased(disk, inode_block);
  for (size_t i = 0; i < data_blocks_count; ++i) {
    assert_block_erased(disk, data_blocks[i]);
  }
  assert_valid_bitmaps(disk);

  size_t free_space_after_delete = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after_delete), CCOS_OK);
  cr_assert_eq(free_space_after_delete, free_space_before);

  free(data_blocks);
  free(file_data);
  ccos_disk_free(disk);
}

Test(bitmap, duplicate_add_rollback) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);

  uint8_t original_data[] = {0x10, 0x20, 0x30, 0x40};
  ccos_inode_t* original = ccos_add_file(disk, subject, original_data, sizeof(original_data), "Alpha~Data~");
  cr_assert_not_null(original, "ccos_add_file failed");
  assert_valid_bitmaps(disk);

  uint8_t* bitmap_before = NULL;
  size_t bitmap_before_size = copy_bitmap_bytes(disk, &bitmap_before);

  uint8_t duplicate_data[] = {0xAA, 0xBB, 0xCC};
  ccos_inode_t* duplicate = ccos_add_file(disk, subject, duplicate_data, sizeof(duplicate_data), "alpha~DATA~");
  cr_assert_null(duplicate, "Case-only duplicate should be rejected");

  assert_bitmap_bytes_equal(disk, bitmap_before, bitmap_before_size);
  assert_valid_bitmaps(disk);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], original);

  free(entries);
  free(bitmap_before);
  ccos_disk_free(disk);
}
