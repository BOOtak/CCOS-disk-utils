#include <criterion/criterion.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccos_format.h"
#include "ccos_image.h"
#include "ccos_private.h"

static void assert_file_name_preserved(ccos_inode_t* file, const char* expected_name) {
  const short_string_t* name = ccos_get_file_name(file);
  cr_assert_not_null(name);
  cr_assert_eq(name->length, strlen(expected_name));
  cr_assert_eq(strncmp(name->data, expected_name, name->length), 0);
}

static void assert_file_contents(ccos_disk_t* disk, ccos_inode_t* file,
                                 const uint8_t* expected, size_t expected_size) {
  uint8_t* actual = NULL;
  size_t actual_size = 0;

  cr_assert_eq(ccos_read_file(disk, file, &actual, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, expected_size);
  cr_assert_eq(memcmp(actual, expected, expected_size), 0);

  free(actual);
}

static ccos_inode_t* create_subject(ccos_disk_t* disk) {
  ccos_inode_t* root = ccos_get_root_dir(disk);
  cr_assert_not_null(root);

  ccos_inode_t* subject = ccos_create_dir(disk, root, "CaseTest");
  cr_assert_not_null(subject, "ccos_create_dir failed");

  return subject;
}

Test(case_insensitive, subject_type_marks_directory) {
  const char* subject_types[] = {"Subject", "subject", "SUBJECT", "SuBjEcT"};

  for (size_t i = 0; i < sizeof(subject_types) / sizeof(subject_types[0]); ++i) {
    ccos_disk_t* disk = NULL;
    int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
    cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

    ccos_inode_t* root = ccos_get_root_dir(disk);
    cr_assert_not_null(root);

    ccos_inode_t* subject = ccos_create_dir(disk, root, "Programs");
    cr_assert_not_null(subject, "ccos_create_dir failed");
    cr_assert_eq(ccos_rename_file(disk, subject, "Programs", subject_types[i]), CCOS_OK);

    char expected_name[CCOS_MAX_FILE_NAME] = {0};
    snprintf(expected_name, sizeof(expected_name), "Programs~%s~", subject_types[i]);
    assert_file_name_preserved(subject, expected_name);
    cr_assert(ccos_is_dir(subject));

    ccos_disk_free(disk);
  }
}

Test(case_insensitive, duplicate_basename_case_is_rejected_without_allocating_blocks) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);

  uint8_t original_data[] = {0x10, 0x20, 0x30, 0x40};
  ccos_inode_t* original = ccos_add_file(disk, subject, original_data, sizeof(original_data), "Alpha~Data~");
  cr_assert_not_null(original, "ccos_add_file failed");

  size_t free_space_before_duplicate = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before_duplicate), CCOS_OK);

  uint8_t duplicate_data[] = {0xAA, 0xBB, 0xCC};
  ccos_inode_t* duplicate = ccos_add_file(disk, subject, duplicate_data, sizeof(duplicate_data), "alpha~Data~");
  cr_assert_null(duplicate, "Case-only duplicate basename should be rejected");

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], original);
  assert_file_name_preserved(entries[0], "Alpha~Data~");
  assert_file_contents(disk, entries[0], original_data, sizeof(original_data));
  free(entries);

  size_t free_space_after_duplicate = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after_duplicate), CCOS_OK);
  cr_assert_eq(free_space_after_duplicate, free_space_before_duplicate);

  ccos_disk_free(disk);
}

Test(case_insensitive, duplicate_type_case_is_rejected_without_allocating_blocks) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);

  uint8_t original_data[] = {0x10, 0x20, 0x30, 0x40};
  ccos_inode_t* original = ccos_add_file(disk, subject, original_data, sizeof(original_data), "Alpha~Data~");
  cr_assert_not_null(original, "ccos_add_file failed");

  size_t free_space_before_duplicate = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before_duplicate), CCOS_OK);

  uint8_t duplicate_data[] = {0xAA, 0xBB, 0xCC};
  ccos_inode_t* duplicate = ccos_add_file(disk, subject, duplicate_data, sizeof(duplicate_data), "Alpha~DATA~");
  cr_assert_null(duplicate, "Case-only duplicate type should be rejected");

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], original);
  assert_file_name_preserved(entries[0], "Alpha~Data~");
  assert_file_contents(disk, entries[0], original_data, sizeof(original_data));
  free(entries);

  size_t free_space_after_duplicate = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after_duplicate), CCOS_OK);
  cr_assert_eq(free_space_after_duplicate, free_space_before_duplicate);

  ccos_disk_free(disk);
}

Test(case_insensitive, file_can_be_found_with_different_type_case) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);

  uint8_t data[] = {0x44, 0x65, 0x6D, 0x6F};
  ccos_inode_t* created = ccos_add_file(disk, subject, data, sizeof(data), "Demo~TEXT~");
  cr_assert_not_null(created, "ccos_add_file failed");
  assert_file_name_preserved(created, "Demo~TEXT~");

  ccos_inode_t* opened = NULL;
  cr_assert_eq(ccos_find_file_by_name(disk, subject, "Demo~Text~", &opened), CCOS_OK);
  cr_assert_not_null(opened, "Unable to find Demo~TEXT~ as Demo~Text~");
  cr_assert_eq(opened, created);
  assert_file_name_preserved(opened, "Demo~TEXT~");
  assert_file_contents(disk, opened, data, sizeof(data));

  ccos_disk_free(disk);
}

Test(case_insensitive, directory_entries_are_sorted_case_insensitively) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);

  uint8_t data[] = {0x42};
  ccos_inode_t* beta = ccos_add_file(disk, subject, data, sizeof(data), "beta~Data~");
  cr_assert_not_null(beta, "ccos_add_file failed");

  ccos_inode_t* alpha = ccos_add_file(disk, subject, data, sizeof(data), "Alpha~Data~");
  cr_assert_not_null(alpha, "ccos_add_file failed");

  ccos_inode_t* gamma = ccos_add_file(disk, subject, data, sizeof(data), "GAMMA~Data~");
  cr_assert_not_null(gamma, "ccos_add_file failed");

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 3);

  cr_assert_eq(entries[0], alpha);
  cr_assert_eq(entries[1], beta);
  cr_assert_eq(entries[2], gamma);
  assert_file_name_preserved(entries[0], "Alpha~Data~");
  assert_file_name_preserved(entries[1], "beta~Data~");
  assert_file_name_preserved(entries[2], "GAMMA~Data~");

  free(entries);
  ccos_disk_free(disk);
}

Test(case_insensitive, type_prefixes_follow_grid_order) {
  const char* names[] = {"App~Run~", "App~Run Report~", "App~Run Report Extra~"};
  const size_t orders[][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};

  for (size_t i = 0; i < sizeof(orders) / sizeof(orders[0]); ++i) {
    ccos_disk_t* disk = NULL;
    int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
    cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

    ccos_inode_t* subject = create_subject(disk);
    uint8_t data[] = {0x42};
    for (size_t j = 0; j < 3; ++j) {
      cr_assert_not_null(ccos_add_file(disk, subject, data, sizeof(data), names[orders[i][j]]));
    }

    uint16_t entry_count = 0;
    ccos_inode_t** entries = NULL;
    cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
    cr_assert_eq(entry_count, 3);
    for (size_t j = 0; j < entry_count; ++j) {
      assert_file_name_preserved(entries[j], names[j]);
    }

    free(entries);
    ccos_disk_free(disk);
  }
}

static void assert_grid_character_order(disk_format_t format, size_t image_size) {
  const char* names[] = {"App~Data~", "Appa~Data~", "Z~Data~", "[~Data~", "\\~Data~",
                         "]~Data~", "^~Data~", "_~Data~", "`~Data~"};
  const size_t name_count = sizeof(names) / sizeof(names[0]);

  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(format, image_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  uint8_t data[] = {0x42};
  for (size_t i = name_count; i > 0; --i) {
    cr_assert_not_null(ccos_add_file(disk, subject, data, sizeof(data), names[i - 1]));
  }

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, name_count);
  for (size_t i = 0; i < entry_count; ++i) {
    assert_file_name_preserved(entries[i], names[i]);
  }

  free(entries);
  ccos_disk_free(disk);
}

Test(case_insensitive, directory_entries_follow_grid_character_order) {
  assert_grid_character_order(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024);
}

Test(case_insensitive, directory_entries_follow_grid_character_order_256) {
  assert_grid_character_order(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}

static ccos_inode_t* create_legacy_subject(ccos_disk_t* disk) {
  ccos_inode_t* subject = create_subject(disk);
  uint8_t data[] = {0x42};
  cr_assert_not_null(ccos_add_file(disk, subject, data, sizeof(data), "App~Run~"));
  cr_assert_not_null(ccos_add_file(disk, subject, data, sizeof(data), "App~Run Report~"));

  uint8_t* contents = NULL;
  size_t size = 0;
  cr_assert_eq(ccos_read_file(disk, subject, &contents, &size), CCOS_OK);
  parsed_directory_element_t* entries = NULL;
  cr_assert_eq(ccos_parse_directory_data(disk, contents, size, 2, &entries), CCOS_OK);

  // Earlier versions put the longer type before its prefix.
  uint8_t* legacy = calloc(size, 1);
  cr_assert_not_null(legacy);
  memcpy(legacy + 1, contents + entries[1].offset, entries[1].size);
  size_t offset = 1 + entries[1].size + 1;
  memcpy(legacy + offset, contents + entries[0].offset, entries[0].size);
  legacy[offset + entries[0].size] = CCOS_DIR_LAST_ENTRY_MARKER;
  cr_assert_eq(ccos_write_file(disk, subject, legacy, size), CCOS_OK);

  free(legacy);
  free(entries);
  free(contents);
  return subject;
}

static void assert_files_in_legacy_order_can_be_deleted(disk_format_t format, size_t image_size) {
  const char* names[] = {"App~Run~", "App~Run Report~"};
  for (size_t i = 0; i < 2; ++i) {
    ccos_disk_t* disk = NULL;
    cr_assert_eq(ccos_new_disk_image(format, image_size, &disk), 0);
    ccos_inode_t* subject = create_legacy_subject(disk);
    ccos_inode_t* file = NULL;
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[i], &file), CCOS_OK);
    cr_assert_eq(ccos_delete_file(disk, file), CCOS_OK);
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[i], &file), CCOS_ENOENT);
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[1 - i], &file), CCOS_OK);
    cr_assert_eq(subject->desc.dir_count, 1);
    cr_assert_eq(ccos_validate_file(disk, subject), CCOS_OK);
    cr_assert(ccos_validate_disk_bitmap(disk));
    ccos_disk_free(disk);
  }
}

static void assert_files_in_legacy_order_can_be_renamed(disk_format_t format, size_t image_size) {
  const char* names[] = {"App~Run~", "App~Run Report~"};
  const char* renamed[] = {"Renamed~Run~", "Renamed~Run Report~"};
  for (size_t i = 0; i < 2; ++i) {
    ccos_disk_t* disk = NULL;
    cr_assert_eq(ccos_new_disk_image(format, image_size, &disk), 0);
    ccos_inode_t* subject = create_legacy_subject(disk);
    ccos_inode_t* file = NULL;
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[i], &file), CCOS_OK);
    cr_assert_eq(ccos_rename_file(disk, file, "Renamed", NULL), CCOS_OK);
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[i], &file), CCOS_ENOENT);
    cr_assert_eq(ccos_find_file_by_name(disk, subject, renamed[i], &file), CCOS_OK);
    cr_assert_eq(ccos_find_file_by_name(disk, subject, names[1 - i], &file), CCOS_OK);
    cr_assert_eq(subject->desc.dir_count, 2);
    cr_assert_eq(ccos_validate_file(disk, subject), CCOS_OK);
    cr_assert(ccos_validate_disk_bitmap(disk));
    ccos_disk_free(disk);
  }
}

static void assert_duplicates_in_legacy_order_are_rejected(disk_format_t format, size_t image_size) {
  ccos_disk_t* disk = NULL;
  cr_assert_eq(ccos_new_disk_image(format, image_size, &disk), 0);
  ccos_inode_t* subject = create_legacy_subject(disk);
  size_t free_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_before), CCOS_OK);

  uint8_t data[] = {0x42};
  cr_assert_null(ccos_add_file(disk, subject, data, sizeof(data), "app~run~"));
  cr_assert_null(ccos_add_file(disk, subject, data, sizeof(data), "app~run report~"));
  cr_assert_eq(subject->desc.dir_count, 2);
  size_t free_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_after), CCOS_OK);
  cr_assert_eq(free_after, free_before);
  cr_assert(ccos_validate_disk_bitmap(disk));
  ccos_disk_free(disk);
}

Test(case_insensitive, files_in_legacy_order_can_be_deleted) {
  assert_files_in_legacy_order_can_be_deleted(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024);
}

Test(case_insensitive, files_in_legacy_order_can_be_deleted_256) {
  assert_files_in_legacy_order_can_be_deleted(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}

Test(case_insensitive, files_in_legacy_order_can_be_renamed) {
  assert_files_in_legacy_order_can_be_renamed(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024);
}

Test(case_insensitive, files_in_legacy_order_can_be_renamed_256) {
  assert_files_in_legacy_order_can_be_renamed(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}

Test(case_insensitive, duplicates_in_legacy_order_are_rejected) {
  assert_duplicates_in_legacy_order_are_rejected(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024);
}

Test(case_insensitive, duplicates_in_legacy_order_are_rejected_256) {
  assert_duplicates_in_legacy_order_are_rejected(CCOS_DISK_FORMAT_BUBMEM, 3 * 128 * 1024);
}

Test(case_insensitive, file_with_extended_type_can_be_deleted) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  uint8_t data[] = {0x42};
  ccos_inode_t* longer = ccos_add_file(disk, subject, data, sizeof(data), "App~Run Report~");
  cr_assert_not_null(longer, "ccos_add_file failed");
  ccos_inode_t* shorter = ccos_add_file(disk, subject, data, sizeof(data), "App~Run~");
  cr_assert_not_null(shorter, "ccos_add_file failed");

  cr_assert_eq(ccos_delete_file(disk, longer), CCOS_OK);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, subject, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, 1);
  cr_assert_eq(entries[0], shorter);
  assert_file_contents(disk, shorter, data, sizeof(data));

  free(entries);
  ccos_disk_free(disk);
}

Test(case_insensitive, file_with_extended_type_can_be_renamed) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  uint8_t data[] = {0x42};
  ccos_inode_t* longer = ccos_add_file(disk, subject, data, sizeof(data), "App~Run Report~");
  cr_assert_not_null(longer, "ccos_add_file failed");
  ccos_inode_t* shorter = ccos_add_file(disk, subject, data, sizeof(data), "App~Run~");
  cr_assert_not_null(shorter, "ccos_add_file failed");

  cr_assert_eq(ccos_rename_file(disk, longer, "Renamed", NULL), CCOS_OK);
  assert_file_name_preserved(longer, "Renamed~Run Report~");
  assert_file_name_preserved(shorter, "App~Run~");
  assert_file_contents(disk, longer, data, sizeof(data));

  ccos_inode_t* found = NULL;
  cr_assert_eq(ccos_find_file_by_name(disk, subject, "App~Run Report~", &found), CCOS_ENOENT);
  cr_assert_eq(ccos_find_file_by_name(disk, subject, "Renamed~Run Report~", &found), CCOS_OK);
  cr_assert_eq(found, longer);
  cr_assert_eq(subject->desc.dir_count, 2);

  ccos_disk_free(disk);
}

Test(case_insensitive, duplicate_extended_type_is_rejected_without_leaking_blocks) {
  ccos_disk_t* disk = NULL;
  int ret = ccos_new_disk_image(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");

  ccos_inode_t* subject = create_subject(disk);
  uint8_t data[] = {0x42};
  ccos_inode_t* longer = ccos_add_file(disk, subject, data, sizeof(data), "App~Run Report~");
  cr_assert_not_null(longer, "ccos_add_file failed");
  cr_assert_not_null(ccos_add_file(disk, subject, data, sizeof(data), "App~Run~"));

  size_t free_space_before = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_before), CCOS_OK);

  ccos_inode_t* duplicate = ccos_add_file(disk, subject, data, sizeof(data), "App~Run Report~");
  cr_assert_null(duplicate, "Duplicate type should be rejected even when its prefix exists");
  cr_assert_eq(subject->desc.dir_count, 2);
  assert_file_contents(disk, longer, data, sizeof(data));

  size_t free_space_after = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space_after), CCOS_OK);
  cr_assert_eq(free_space_after, free_space_before);

  ccos_disk_free(disk);
}
