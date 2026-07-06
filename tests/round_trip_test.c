#include <criterion/criterion.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ccos_format.h"
#include "ccos_image.h"

static uint8_t* create_test_data(size_t size) {
  uint8_t* data = malloc(size);
  if (data == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < size; ++i) {
    data[i] = (uint8_t)((i * 31u + i / 7u + 0x5Au) & 0xFFu);
  }

  return data;
}

static ccos_inode_t* find_dir_entry(ccos_inode_t** entries, uint16_t entry_count, ccos_inode_t* file) {
  for (uint16_t i = 0; i < entry_count; ++i) {
    if (entries[i] == file) {
      return entries[i];
    }
  }

  return NULL;
}

static void assert_file_name(ccos_inode_t* file, const char* expected_name) {
  const short_string_t* name = ccos_get_file_name(file);
  cr_assert_not_null(name);
  cr_assert_eq(name->length, strlen(expected_name));
  cr_assert_eq(strncmp(name->data, expected_name, name->length), 0);
}

static void assert_file_contents(ccos_disk_t* disk, ccos_inode_t* file, const uint8_t* expected, size_t expected_size) {
  uint8_t* actual = NULL;
  size_t actual_size = 0;

  cr_assert_eq(ccos_read_file(disk, file, &actual, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, expected_size);
  cr_assert_eq(memcmp(actual, expected, expected_size), 0);

  free(actual);
}

static void assert_round_trip(disk_format_t format, size_t image_size, uint16_t expected_sector_size,
                              size_t file_size, const char* file_name) {
  ccos_disk_t disk;
  int ret = ccos_new_disk_image(format, image_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");
  cr_assert_eq(disk.sector_size, expected_sector_size);

  uint8_t* expected = create_test_data(file_size);
  cr_assert_not_null(expected, "Failed to allocate test data");

  ccos_inode_t* root = ccos_get_root_dir(&disk);
  cr_assert_not_null(root, "Failed to get root directory");

  ccos_inode_t* subject = ccos_create_dir(&disk, root, "RoundTrip");
  cr_assert_not_null(subject, "ccos_create_dir failed");
  cr_assert_eq(ccos_validate_file(&disk, subject), CCOS_OK);
  cr_assert_eq(ccos_validate_file(&disk, root), CCOS_OK);
  assert_file_name(subject, "RoundTrip~Subject~");

  uint16_t root_entry_count = 0;
  ccos_inode_t** root_entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(&disk, root, &root_entry_count, &root_entries), CCOS_OK);
  cr_assert_eq(root_entry_count, 1);
  cr_assert_not_null(find_dir_entry(root_entries, root_entry_count, subject),
                     "Subject directory is missing from root directory");
  free(root_entries);

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(&disk, &free_space_before), CCOS_OK);

  ccos_inode_t* file = ccos_add_file(&disk, subject, expected, file_size, file_name);
  cr_assert_not_null(file, "ccos_add_file failed");

  cr_assert_eq(file->desc.file_size, file_size);
  cr_assert_eq(file->desc.dir_file_id, subject->header.file_id);
  assert_file_name(file, file_name);
  cr_assert_eq(ccos_validate_file(&disk, file), CCOS_OK);
  cr_assert_eq(ccos_validate_file(&disk, subject), CCOS_OK);

  root_entry_count = 0;
  root_entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(&disk, root, &root_entry_count, &root_entries), CCOS_OK);
  cr_assert_eq(root_entry_count, 1);
  cr_assert_null(find_dir_entry(root_entries, root_entry_count, file), "Added file should not be in root directory");
  cr_assert_not_null(find_dir_entry(root_entries, root_entry_count, subject),
                     "Subject directory is missing from root directory");
  free(root_entries);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(&disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  ccos_inode_t* added_file = find_dir_entry(entries, entry_count, file);
  cr_assert_not_null(added_file, "Added file is missing from subject directory");
  assert_file_name(added_file, file_name);

  size_t free_space_after_add = 0;
  cr_assert_eq(ccos_calc_free_space(&disk, &free_space_after_add), CCOS_OK);
  cr_assert_lt(free_space_after_add, free_space_before);

  assert_file_contents(&disk, added_file, expected, file_size);
  free(entries);

  cr_assert_eq(ccos_delete_file(&disk, file), CCOS_OK);
  cr_assert_eq(ccos_validate_file(&disk, subject), CCOS_OK);

  entry_count = 0;
  entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(&disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 0);
  free(entries);

  size_t free_space_after_delete = 0;
  cr_assert_eq(ccos_calc_free_space(&disk, &free_space_after_delete), CCOS_OK);
  cr_assert_eq(free_space_after_delete, free_space_before);

  free(expected);
  free(disk.data);
}

Test(round_trip, small_file_less_than_sector) {
  assert_round_trip(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, 512, 128, "Small~Data~");
}

Test(round_trip, medium_file_several_sectors) {
  assert_round_trip(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, 512, 4096, "Medium~Data~");
}

Test(round_trip, large_file_up_to_one_megabyte) {
  assert_round_trip(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, 512, 1024 * 1024, "Large~Data~");
}

Test(round_trip, small_file_less_than_256_byte_sector) {
  assert_round_trip(CCOS_DISK_FORMAT_BUBMEM, 2 * 1024 * 1024, 256, 128, "Small256~Data~");
}

Test(round_trip, medium_file_several_256_byte_sectors) {
  assert_round_trip(CCOS_DISK_FORMAT_BUBMEM, 2 * 1024 * 1024, 256, 4096, "Medium256~Data~");
}

Test(round_trip, large_file_up_to_one_megabyte_on_256_byte_sectors) {
  assert_round_trip(CCOS_DISK_FORMAT_BUBMEM, 2 * 1024 * 1024, 256, 1024 * 1024, "Large256~Data~");
}
