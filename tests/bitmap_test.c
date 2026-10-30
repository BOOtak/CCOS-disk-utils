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
    memcpy(*snapshot + i * bitmask_size, ccos_get_bitmask_bytes(ccos_get_bitmask_blocks(disk, &bitmask_list)[i]), bitmask_size);
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
  uint8_t* bytes = ccos_get_bitmask_bytes(ccos_get_bitmask_blocks(disk, &bitmask_list)[bitmask_index]);
  bytes[local_block / 8] &= ~(1u << (local_block % 8));

  ccos_update_bitmask_checksum(disk, ccos_get_bitmask_blocks(disk, &bitmask_list)[bitmask_index]);
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
  ccos_get_bitmask_blocks(disk, &bitmask_list)[0]->allocated = block_count + 1;
  ccos_update_bitmask_checksum(disk, ccos_get_bitmask_blocks(disk, &bitmask_list)[0]);

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

  ccos_get_bitmask_blocks(disk, &bitmask_list)[0]->allocated = 0;
  ccos_update_bitmask_checksum(disk, ccos_get_bitmask_blocks(disk, &bitmask_list)[0]);

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

  ccos_get_bitmask_blocks(disk, &bitmask_list)[0]->allocated = 0;
  ccos_update_bitmask_checksum(disk, ccos_get_bitmask_blocks(disk, &bitmask_list)[0]);

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

  ccos_bitmask_t* bitmask = ccos_get_bitmask_blocks(disk, &bitmask_list)[0];
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

Test(bitmap, inode_block_list_boundary) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  cr_assert_eq(ccos_get_inode_max_sectors(disk), 146);
  cr_assert_eq(sizeof(ccos_inode_t), 512);
  cr_assert_eq(offsetof(ccos_inode_t, content.bs512.padding), 508);

  ccos_inode_t* subject = create_subject(disk);
  assert_valid_bitmaps(disk);

  const size_t file_size = 147 * ccos_get_log_sector_size(disk);
  uint8_t* file_data = create_test_data(file_size);
  cr_assert_not_null(file_data);

  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, file_size, "Boundary~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");
  cr_assert_neq(file->content_inode_info.block_next, CCOS_INVALID_BLOCK);

  // Padding must not be interpreted as additional block numbers.
  memset(file->content.bs512.padding, 0, sizeof(file->content.bs512.padding));
  cr_assert(ccos_is_valid_inode_checksum(disk, file));

  size_t data_blocks_count = 0;
  uint16_t* data_blocks = NULL;
  cr_assert_eq(ccos_get_file_sectors(disk, file, &data_blocks_count, &data_blocks), CCOS_OK);
  cr_assert_eq(data_blocks_count, 147);

  assert_valid_bitmaps(disk);

  free(data_blocks);
  free(file_data);
  ccos_disk_free(disk);
}

Test(bitmap, content_inode_fragment_indices) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  const size_t content_inode_count = 3;
  const size_t data_blocks_count = ccos_get_inode_max_sectors(disk)
    + (content_inode_count - 1) * ccos_get_content_inode_max_sectors(disk) + 1;
  const size_t file_size = data_blocks_count * ccos_get_log_sector_size(disk);
  uint8_t* file_data = create_test_data(file_size);
  cr_assert_not_null(file_data);

  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, file_size, "Indices~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");
  cr_assert_eq(file->content_inode_info.header.file_fragment_index, 0);
  cr_assert(ccos_is_valid_inode_checksum(disk, file));

  uint16_t block = file->content_inode_info.block_next;
  for (size_t i = 1; i <= content_inode_count; ++i) {
    cr_assert_neq(block, CCOS_INVALID_BLOCK);
    ccos_content_inode_t* content_inode = ccos_disk_read(disk, block);
    cr_assert_not_null(content_inode);
    cr_assert_eq(content_inode->content_inode_info.header.file_id, file->header.file_id);
    cr_assert_eq(content_inode->content_inode_info.header.file_fragment_index, i);
    cr_assert_eq(content_inode->content_inode_info.blocks_checksum,
                 ccos_calc_content_inode_checksum(disk, content_inode));
    block = content_inode->content_inode_info.block_next;
  }
  cr_assert_eq(block, CCOS_INVALID_BLOCK);

  assert_valid_bitmaps(disk);

  free(file_data);
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

Test(bitmap, duplicate_copy_rollback) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  uint8_t file_data[] = {0x10, 0x20, 0x30, 0x40};
  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, sizeof(file_data), "Copy~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);
  uint8_t* bitmap_before = NULL;
  size_t bitmap_before_size = copy_bitmap_bytes(disk, &bitmap_before);

  cr_assert_eq(ccos_copy_file(disk, file, disk, subject), CCOS_EEXIST);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], file);
  free(entries);

  uint8_t* actual = NULL;
  size_t actual_size = 0;
  cr_assert_eq(ccos_read_file(disk, file, &actual, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, sizeof(file_data));
  cr_assert_eq(memcmp(actual, file_data, actual_size), 0);
  free(actual);

  assert_valid_bitmaps(disk);
  size_t free_space_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after), CCOS_OK);
  cr_assert_eq(free_space_after, free_space_before);
  assert_bitmap_bytes_equal(disk, bitmap_before, bitmap_before_size);

  free(bitmap_before);
  ccos_disk_free(disk);
}

Test(bitmap, copy_read_error_rollback) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  ccos_inode_t* destination = ccos_create_dir(disk, ccos_get_root_dir(disk), "Destination");
  cr_assert_not_null(destination, "ccos_create_dir failed");
  uint8_t file_data[] = {0x10, 0x20, 0x30, 0x40};
  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, sizeof(file_data), "Copy~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");

  // A continuation block outside the image makes reading the source fail.
  file->content_inode_info.block_next = ccos_disk_size(disk) / ccos_disk_sector_size(disk);
  ccos_update_inode_checksums(disk, file);

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);
  uint8_t* bitmap_before = NULL;
  size_t bitmap_before_size = copy_bitmap_bytes(disk, &bitmap_before);

  cr_assert_eq(ccos_copy_file(disk, file, disk, destination), CCOS_EIO);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, destination, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 0);
  free(entries);
  cr_assert_eq(ccos_validate_file(disk, destination), CCOS_OK);

  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], file);
  free(entries);

  assert_valid_bitmaps(disk);
  size_t free_space_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after), CCOS_OK);
  cr_assert_eq(free_space_after, free_space_before);
  assert_bitmap_bytes_equal(disk, bitmap_before, bitmap_before_size);

  free(bitmap_before);
  ccos_disk_free(disk);
}

static void assert_copy_no_space_rollback(disk_format_t format, size_t image_size,
                                          size_t data_blocks_count, size_t remaining_blocks) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(format, image_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  ccos_inode_t* destination = ccos_create_dir(disk, ccos_get_root_dir(disk), "Destination");
  cr_assert_not_null(destination, "ccos_create_dir failed");

  size_t file_size = data_blocks_count * ccos_get_log_sector_size(disk);
  uint8_t* file_data = create_test_data(file_size);
  cr_assert_not_null(file_data);
  ccos_inode_t* file = ccos_add_file(disk, subject, file_data, file_size, "Copy~Data~");
  cr_assert_not_null(file, "ccos_add_file failed");

  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  size_t free_blocks_count = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks_count), CCOS_OK);
  cr_assert_geq(free_blocks_count, remaining_blocks);
  while (free_blocks_count > remaining_blocks) {
    uint16_t block = ccos_get_free_sector(disk, &bitmask_list);
    cr_assert_neq(block, CCOS_INVALID_BLOCK);
    ccos_mark_sector(disk, &bitmask_list, block, 1);
    free_blocks_count--;
  }

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);
  cr_assert_eq(free_space_before, remaining_blocks * ccos_disk_sector_size(disk));
  uint8_t* bitmap_before = NULL;
  size_t bitmap_before_size = copy_bitmap_bytes(disk, &bitmap_before);

  cr_assert_eq(ccos_copy_file(disk, file, disk, destination), CCOS_ENOSPC);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, destination, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 0);
  free(entries);
  cr_assert_eq(ccos_validate_file(disk, destination), CCOS_OK);
  cr_assert_eq(ccos_validate_file(disk, file), CCOS_OK);

  uint8_t* actual = NULL;
  size_t actual_size = 0;
  cr_assert_eq(ccos_read_file(disk, file, &actual, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, file_size);
  cr_assert_eq(memcmp(actual, file_data, file_size), 0);
  free(actual);

  assert_valid_bitmaps(disk);
  size_t free_space_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after), CCOS_OK);
  cr_assert_eq(free_space_after, free_space_before);
  assert_bitmap_bytes_equal(disk, bitmap_before, bitmap_before_size);

  free(bitmap_before);
  free(file_data);
  ccos_disk_free(disk);
}

Test(bitmap, copy_no_space_rollback) {
  // Leave room for the copied header and only one of its two data blocks.
  assert_copy_no_space_rollback(CCOS_DISK_FORMAT_COMPASS, 720 * 512, 2, 2);
}

Test(bitmap, copy_no_space_rollback_256) {
  assert_copy_no_space_rollback(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024, 2, 2);
}

Test(bitmap, copy_no_space_for_content_inode_rollback) {
  // The last free block holds data, leaving no room for its continuation FAR.
  assert_copy_no_space_rollback(CCOS_DISK_FORMAT_COMPASS, 720 * 512,
                                BS512_INODE_MAX_BLOCKS + 1, BS512_INODE_MAX_BLOCKS + 2);
}

Test(bitmap, copy_no_space_for_content_inode_rollback_256) {
  assert_copy_no_space_rollback(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024,
                                BS256_INODE_MAX_BLOCKS + 1, BS256_INODE_MAX_BLOCKS + 2);
}

static void assert_copy_directory_no_space_rollback(disk_format_t format, size_t image_size) {
  ccos_disk_t* disk = NULL;
  cr_assert_eq(ccos_new_disk_image(format, image_size, &disk), 0);
  ccos_inode_t* subject = create_subject(disk);
  ccos_inode_t* destination = ccos_create_dir(disk, ccos_get_root_dir(disk), "Destination");
  cr_assert_not_null(destination);

  char name[CCOS_MAX_FILE_NAME];
  memset(name, 'X', sizeof(name));
  memcpy(name + sizeof(name) - 7, "~Data~", 7);
  size_t entry_size = strlen(name) + sizeof(dir_entry_t) + 2;
  size_t entry_count = (ccos_get_log_sector_size(disk) - 1) / entry_size;
  uint8_t data[] = {0x42};
  for (size_t i = 0; i < entry_count; ++i) {
    name[0] = 'A' + i;
    cr_assert_not_null(ccos_add_file(disk, destination, data, sizeof(data), name));
  }
  name[0] = 'Z';
  ccos_inode_t* file = ccos_add_file(disk, subject, data, sizeof(data), name);
  cr_assert_not_null(file);

  uint8_t* directory_before = NULL;
  size_t directory_size = 0;
  cr_assert_eq(ccos_read_file(disk, destination, &directory_before, &directory_size), CCOS_OK);
  cr_assert_leq(directory_size, ccos_get_log_sector_size(disk));
  cr_assert_gt(directory_size + entry_size, ccos_get_log_sector_size(disk));
  uint8_t inode_before[BS512_BLOCK_SIZE];
  memcpy(inode_before, destination, ccos_disk_sector_size(disk));

  // The file fits, but adding its directory entry needs one more sector.
  ccos_bitmask_list_t bitmask_list = ccos_find_bitmask_sectors(disk);
  size_t free_blocks = 0;
  cr_assert_eq(ccos_get_free_sectors_count(disk, &bitmask_list, &free_blocks), CCOS_OK);
  while (free_blocks > 2) {
    uint16_t block = ccos_get_free_sector(disk, &bitmask_list);
    cr_assert_neq(block, CCOS_INVALID_BLOCK);
    ccos_mark_sector(disk, &bitmask_list, block, 1);
    free_blocks--;
  }
  uint8_t* bitmap_before = NULL;
  size_t bitmap_size = copy_bitmap_bytes(disk, &bitmap_before);
  size_t free_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_before), CCOS_OK);
  cr_assert_eq(free_before, 2 * ccos_disk_sector_size(disk));

  cr_assert_eq(ccos_copy_file(disk, file, disk, destination), CCOS_ENOSPC);

  uint8_t* directory_after = NULL;
  size_t actual_size = 0;
  cr_assert_eq(ccos_read_file(disk, destination, &directory_after, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, directory_size);
  cr_assert_eq(memcmp(directory_after, directory_before, directory_size), 0);
  cr_assert_eq(memcmp(destination, inode_before, ccos_disk_sector_size(disk)), 0);
  assert_bitmap_bytes_equal(disk, bitmap_before, bitmap_size);
  assert_valid_bitmaps(disk);
  size_t free_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_after), CCOS_OK);
  cr_assert_eq(free_after, free_before);
  cr_assert_eq(ccos_validate_file(disk, file), CCOS_OK);

  free(directory_after);
  free(directory_before);
  free(bitmap_before);
  ccos_disk_free(disk);
}

Test(bitmap, copy_directory_no_space_rollback) {
  assert_copy_directory_no_space_rollback(CCOS_DISK_FORMAT_COMPASS, 720 * 512);
}

Test(bitmap, copy_directory_no_space_rollback_256) {
  assert_copy_directory_no_space_rollback(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}
