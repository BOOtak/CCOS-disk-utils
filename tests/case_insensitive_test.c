#include <criterion/criterion.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ccos_format.h"
#include "ccos_image.h"

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
