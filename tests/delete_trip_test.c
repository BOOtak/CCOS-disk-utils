#include <criterion/criterion.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ccos_format.h"
#include "ccos_image.h"
#include "ccos_private.h"

#define FILE_COUNT 5
#define DELETE_ADD_CYCLES 3

typedef struct {
  const char* name;
  size_t size;
  uint8_t* data;
  ccos_inode_t* inode;
} test_file_t;

typedef struct {
  uint16_t* blocks;
  size_t count;
} block_snapshot_t;

static uint8_t* create_random_data(size_t size) {
  uint8_t* data = malloc(size);
  if (data == NULL) {
    return NULL;
  }

  for (size_t i = 0; i < size; ++i) {
    data[i] = (uint8_t)rand();
  }

  return data;
}

static ccos_inode_t* create_programs_dir(ccos_disk_t* disk) {
  ccos_inode_t* root = ccos_get_root_dir(disk);
  cr_assert_not_null(root);

  ccos_inode_t* programs = ccos_create_dir(disk, root, "Programs");
  cr_assert_not_null(programs, "ccos_create_dir failed");
  cr_assert_eq(ccos_validate_file(disk, root), CCOS_OK);
  cr_assert_eq(ccos_validate_file(disk, programs), CCOS_OK);

  return programs;
}

static void assert_block_free_and_erased(ccos_disk_t* disk, uint16_t block) {
  uint32_t* block_data = ccos_disk_read(disk, block);
  cr_assert_not_null(block_data);
  cr_assert_eq(*block_data, CCOS_EMPTY_BLOCK_MARKER, "Block 0x%x is not erased", block);

  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(disk);
  cr_assert_gt(bitmask_list.length, 0);

  size_t bitmask_blocks = get_bitmask_blocks(disk);
  size_t bitmask_index = block / bitmask_blocks;
  size_t local_block = block - bitmask_index * bitmask_blocks;
  cr_assert_lt(bitmask_index, bitmask_list.length);

  uint8_t* bitmask_bytes = get_bitmask_bytes(bitmask_list.bitmask_blocks[bitmask_index]);
  cr_assert_eq(bitmask_bytes[local_block / 8] & (1u << (local_block % 8)), 0,
               "Block 0x%x is still marked as allocated", block);
}

static void append_snapshot_block(block_snapshot_t* snapshot, uint16_t block) {
  uint16_t* new_blocks = realloc(snapshot->blocks, (snapshot->count + 1) * sizeof(uint16_t));
  cr_assert_not_null(new_blocks);

  snapshot->blocks = new_blocks;
  snapshot->blocks[snapshot->count++] = block;
}

static block_snapshot_t collect_file_blocks(ccos_disk_t* disk, ccos_inode_t* file) {
  block_snapshot_t snapshot = {0};
  append_snapshot_block(&snapshot, file->header.file_id);

  size_t data_blocks_count = 0;
  uint16_t* data_blocks = NULL;
  cr_assert_eq(get_file_blocks(disk, file, &data_blocks_count, &data_blocks), CCOS_OK);
  for (size_t i = 0; i < data_blocks_count; ++i) {
    append_snapshot_block(&snapshot, data_blocks[i]);
  }
  free(data_blocks);

  uint16_t content_inode_block = file->content_inode_info.block_next;
  while (content_inode_block != CCOS_INVALID_BLOCK) {
    append_snapshot_block(&snapshot, content_inode_block);

    ccos_content_inode_t* content_inode = ccos_disk_read(disk, content_inode_block);
    cr_assert_not_null(content_inode);
    content_inode_block = content_inode->content_inode_info.block_next;
  }

  return snapshot;
}

static void assert_snapshot_blocks_freed(ccos_disk_t* disk, const block_snapshot_t* snapshot) {
  for (size_t i = 0; i < snapshot->count; ++i) {
    assert_block_free_and_erased(disk, snapshot->blocks[i]);
  }
}

static void assert_file_contents(ccos_disk_t* disk, ccos_inode_t* file, const uint8_t* expected, size_t expected_size) {
  uint8_t* actual = NULL;
  size_t actual_size = 0;

  cr_assert_eq(ccos_read_file(disk, file, &actual, &actual_size), CCOS_OK);
  cr_assert_eq(actual_size, expected_size);
  cr_assert_eq(memcmp(actual, expected, expected_size), 0);

  free(actual);
}

static void assert_directory_state(ccos_disk_t* disk, ccos_inode_t* programs, test_file_t* files) {
  cr_assert(validate_disk_bitmap(disk));
  size_t free_space = 0;
  cr_assert_eq(ccos_calc_free_space(disk, &free_space), CCOS_OK);

  ccos_inode_t* root = ccos_get_root_dir(disk);
  cr_assert_not_null(root);
  cr_assert_eq(ccos_validate_file(disk, root), CCOS_OK);
  cr_assert_eq(ccos_validate_file(disk, programs), CCOS_OK);

  uint16_t entry_count = 0;
  ccos_inode_t** entries = NULL;
  cr_assert_eq(ccos_get_dir_contents(disk, programs, &entry_count, &entries), CCOS_OK);
  cr_assert_eq(entry_count, FILE_COUNT);

  for (size_t i = 0; i < FILE_COUNT; ++i) {
    cr_assert_eq(ccos_validate_file(disk, files[i].inode), CCOS_OK);
    assert_file_contents(disk, files[i].inode, files[i].data, files[i].size);

    bool found = false;
    for (uint16_t j = 0; j < entry_count; ++j) {
      if (entries[j] == files[i].inode) {
        found = true;
        break;
      }
    }
    cr_assert(found, "File %s is missing from Programs", files[i].name);
  }

  free(entries);
}

static void run_delete_trip(disk_format_t format, size_t image_size, uint16_t expected_sector_size) {
  static const char* names[FILE_COUNT] = {
    "Alpha~Data~",
    "Beta~Data~",
    "Gamma~Data~",
    "Delta~Data~",
    "Epsilon~Data~",
  };

  ccos_disk_t disk;
  int ret = ccos_new_disk_image(format, image_size, &disk);
  cr_assert_eq(ret, 0, "ccos_new_disk_image failed");
  cr_assert_eq(disk.sector_size, expected_sector_size);

  const size_t data_block_size = get_log_block_size(&disk);
  const size_t sizes[FILE_COUNT] = {
    16,
    data_block_size,
    data_block_size + 1,
    data_block_size * 2,
    data_block_size * 4,
  };

  test_file_t files[FILE_COUNT] = {0};
  ccos_inode_t* programs = create_programs_dir(&disk);

  for (size_t i = 0; i < FILE_COUNT; ++i) {
    files[i].name = names[i];
    files[i].size = sizes[i];
    files[i].data = create_random_data(files[i].size);
    cr_assert_not_null(files[i].data);

    files[i].inode = ccos_add_file(&disk, programs, files[i].data, files[i].size, files[i].name);
    cr_assert_not_null(files[i].inode, "ccos_add_file failed for %s", files[i].name);
  }

  assert_directory_state(&disk, programs, files);

  const size_t order[FILE_COUNT] = {1, 0, 2, 3, 4};
  for (size_t order_index = 0; order_index < FILE_COUNT; ++order_index) {
    size_t file_index = order[order_index];

    for (size_t cycle = 0; cycle < DELETE_ADD_CYCLES; ++cycle) {
      block_snapshot_t snapshot = collect_file_blocks(&disk, files[file_index].inode);

      cr_assert_eq(ccos_delete_file(&disk, files[file_index].inode), CCOS_OK);
      assert_snapshot_blocks_freed(&disk, &snapshot);
      cr_assert(validate_disk_bitmap(&disk));

      files[file_index].inode = ccos_add_file(&disk, programs, files[file_index].data, files[file_index].size,
                                              files[file_index].name);
      cr_assert_not_null(files[file_index].inode, "Re-add failed for %s", files[file_index].name);
      assert_directory_state(&disk, programs, files);

      free(snapshot.blocks);
    }
  }

  assert_directory_state(&disk, programs, files);

  for (size_t i = 0; i < FILE_COUNT; ++i) {
    free(files[i].data);
  }
  free(disk.data);
}

Test(delete_trip, repeated_delete_add_512_byte_sectors) {
  run_delete_trip(CCOS_DISK_FORMAT_COMPASS, 10 * 1024 * 1024, 512);
}

Test(delete_trip, repeated_delete_add_256_byte_sectors) {
  run_delete_trip(CCOS_DISK_FORMAT_BUBMEM, 2 * 1024 * 1024, 256);
}
