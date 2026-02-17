#ifndef CCOS_DISK_TOOL_CCOS_STRUCTURE_H
#define CCOS_DISK_TOOL_CCOS_STRUCTURE_H

#include <stddef.h>
#include <stdint.h>
#include "ccos_disk.h"

#if __STDC_VERSION__ >= 201112L
#include <assert.h>
#else
#define static_assert(...)
#endif

#define INODE_BLOCKS_OFFSET (sizeof(ccos_block_header_t) + sizeof(ccos_inode_desc_t) + sizeof(ccos_block_data_t))

#define BS256_BLOCK_SIZE                (256)
#define BS256_LOG_BLOCK_SIZE            (BS256_BLOCK_SIZE - sizeof(ccos_block_header_t))
#define BS256_INODE_MAX_BLOCKS          ((BS256_BLOCK_SIZE - INODE_BLOCKS_OFFSET) / 2)
#define BS256_CONTENT_INODE_PADDING     (4)
#define BS256_CONTENT_INODE_MAX_BLOCKS  ((BS256_BLOCK_SIZE - sizeof(ccos_block_data_t) - BS256_CONTENT_INODE_PADDING) / 2)
#define BS256_BITMASK_PADDING           (0)
#define BS256_BITMASK_SIZE              (BS256_BLOCK_SIZE - sizeof(ccos_block_header_t) - 2 - 2 - BS256_BITMASK_PADDING)
#define BS256_BITMASK_BLOCKS            (BS256_BITMASK_SIZE * 8)
#define BS256_DIR_DEFAULT_SIZE          BS256_LOG_BLOCK_SIZE

#define BS512_BLOCK_SIZE                512
#define BS512_LOG_BLOCK_SIZE            (BS512_BLOCK_SIZE - sizeof(ccos_block_header_t) - 4)
#define BS512_INODE_MAX_BLOCKS          ((BS512_BLOCK_SIZE - INODE_BLOCKS_OFFSET) / 2)
#define BS512_CONTENT_INODE_PADDING     (8)
#define BS512_CONTENT_INODE_MAX_BLOCKS  ((BS512_BLOCK_SIZE - sizeof(ccos_block_data_t) - BS512_CONTENT_INODE_PADDING) / 2)
#define BS512_BITMASK_PADDING           (4)
#define BS512_BITMASK_SIZE              (BS512_BLOCK_SIZE - sizeof(ccos_block_header_t) - 2 - 2 - BS512_BITMASK_PADDING)
#define BS512_BITMASK_BLOCKS            (BS512_BITMASK_SIZE * 8)
#define BS512_DIR_DEFAULT_SIZE          BS512_LOG_BLOCK_SIZE

// Block number is 2 bytes => max blocks = 65535; each bitmask stores 4000 blocks => we need 17 bitmask blocks max
#define MAX_BITMASK_BLOCKS_IN_IMAGE 17

#define CCOS_DIR_ENTRIES_OFFSET 0x1
#define CCOS_DIR_ENTRY_SUFFIX_LENGTH 0x2
#define CCOS_DIR_LAST_ENTRY_MARKER 0xFFU

#define CCOS_MAX_FILE_NAME 80

#define CCOS_INVALID_BLOCK 0xFFFF
#define CCOS_EMPTY_BLOCK_MARKER 0xFFFFFFFF

#define CCOS_DATA_OFFSET sizeof(ccos_block_header_t)

#pragma pack(push, 1)
typedef struct {
  uint8_t boot_indicator;
  uint8_t begin_head;
  uint8_t begin_sector;
  uint8_t begin_cylinder;
  uint8_t system_indicator;
  uint8_t ending_head;
  uint8_t ending_sector;
  uint8_t ending_cylinder;
  uint32_t relative_sector;
  uint32_t num_sectors;
} ccos_boot_sector_partition_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  uint8_t  header[14];
  uint16_t bytes_per_page;
  uint16_t pages_per_track;
  uint16_t tracks_per_cylinder;
  uint16_t num_cylinders;
  uint8_t  second_side_count;
  uint16_t valid_info_flag;
  uint8_t  dummy[5];
  uint16_t bitmap_fid;
  uint16_t superblock_fid;
  uint16_t min_dir_pages;
  uint16_t log_page_size;
  uint8_t  boot_code[406];
  uint16_t partition_indicator;
  ccos_boot_sector_partition_t partitionTable[4];
  uint16_t last_word_flag;
} ccos_boot_sector_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t tenthOfSec;
  uint8_t dayOfWeek;
  uint16_t dayOfYear;
} ccos_date_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  uint16_t file_id;
  uint16_t file_fragment_index;
} ccos_block_header_t;
#pragma pack(pop)

static_assert(sizeof(ccos_block_header_t) == 4, "bad block header size");

#pragma pack(push, 1)
typedef struct {
  ccos_block_header_t header;
  uint16_t blocks_checksum;  // checksum([block_next ... block_end), file_id, file_fragment_index)
  uint16_t block_next;       // next block with content blocks. 0xFFFF if not present
  uint16_t block_current;    // current block with content blocks
  uint16_t block_prev;       // previous block with content blocks. 0xFFFF if not present
} ccos_block_data_t;
#pragma pack(pop)

static_assert(sizeof(ccos_block_data_t) == 12, "bad block data size");

#pragma pack(push, 1)
typedef struct {
  uint32_t file_size;
  uint8_t name_length;
  char name[CCOS_MAX_FILE_NAME];
  ccos_date_t creation_date;
  uint16_t dir_file_id;  // file id of the parent directory
  ccos_date_t mod_date;
  ccos_date_t expiration_date;
  uint32_t machine_ID;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t comp;         // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t encry;        // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t protec;       // write-protected ???
  uint8_t pswd_len;     // unused in 3.0+ ???
  char pswd[4];         // unused in 3.0+ ???
  uint32_t dir_length;  // directory size in bytes. Matches file_size for large dirs, but not always
  uint16_t dir_count;   // number of files in the directory
  uint8_t pad[6];
  uint8_t asc;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t uses_8087;
  uint8_t version_major;
  uint8_t version_minor;
  uint32_t system;  // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint8_t pad2[11];
  uint8_t version_patch;
  uint32_t prop_length;  // indicates how much bytes at the beginning of the file are used to store some properties and
  // are not part of the file
  uint8_t rom;           // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t rom_id;       // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t mode;         // from InteGRiD Sources OSINCS/WSTYPE.INC
  char RDB[3];           // from InteGRiD Sources OSINCS/WSTYPE.INC
  char UDB[20];          // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t grid_central_use;   // from InteGRiD Sources OSINCS/WSTYPE.INC
  uint16_t metadata_checksum;  // checksum([file_id ... metadata_checksum))
} ccos_inode_desc_t;
#pragma pack(pop)

static_assert(sizeof(ccos_inode_desc_t) == 200, "bad descriptor size");

#pragma pack(push, 1)
typedef struct ccos_inode_t_ {
  ccos_block_header_t header;
  ccos_inode_desc_t desc;
  ccos_block_data_t content_inode_info;
  union {
    struct { uint16_t blocks[BS256_INODE_MAX_BLOCKS]; } bs256;
    struct { uint16_t blocks[BS512_INODE_MAX_BLOCKS]; } bs512;
  } content;
} ccos_inode_t;
#pragma pack(pop)

static_assert(sizeof(ccos_inode_t) == 512, "bad inode size");

#pragma pack(push, 1)
typedef struct {
  ccos_block_data_t content_inode_info;
  union {
    struct {
      uint16_t blocks[BS256_CONTENT_INODE_MAX_BLOCKS];
      uint8_t padding[BS256_CONTENT_INODE_PADDING];
    } bs256;
    struct {
      uint16_t blocks[BS512_CONTENT_INODE_MAX_BLOCKS];
      uint8_t padding[BS512_CONTENT_INODE_PADDING];
    } bs512;
  } content;
} ccos_content_inode_t;
#pragma pack(pop)

static_assert(sizeof(ccos_content_inode_t) == 512, "bad inode size");

#pragma pack(push, 1)
typedef struct {
  ccos_block_header_t header;
  uint16_t checksum;
  uint16_t allocated;
  union {
    struct {
      uint8_t bytes[BS256_BITMASK_SIZE];
      uint8_t padding[BS256_BITMASK_PADDING];
    } bs256;
    struct {
      uint8_t bytes[BS512_BITMASK_SIZE];
      uint8_t padding[BS512_BITMASK_PADDING];
    } bs512;
  } content;
} ccos_bitmask_t;
#pragma pack(pop)

static_assert(sizeof(ccos_bitmask_t) == 512, "bad bitmask size");

#pragma pack(push, 1)
typedef struct {
  size_t length;
  ccos_bitmask_t* bitmask_blocks[MAX_BITMASK_BLOCKS_IN_IMAGE];
} ccos_bitmask_list_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
  uint16_t block;
  uint8_t name_length;
} dir_entry_t;
#pragma pack(pop)

size_t get_block_size(ccos_disk_t* disk);
size_t get_log_block_size(ccos_disk_t* disk);
size_t get_inode_max_blocks(ccos_disk_t* disk);
size_t get_content_inode_padding(ccos_disk_t* disk);
size_t get_content_inode_max_blocks(ccos_disk_t* disk);
size_t get_bitmask_size(ccos_disk_t* disk);
size_t get_bitmask_blocks(ccos_disk_t* disk);
size_t get_dir_default_size(ccos_disk_t* disk);

uint16_t* get_inode_content_blocks(ccos_inode_t* inode);
uint16_t* get_content_inode_content_blocks(ccos_content_inode_t* inode);
uint8_t* get_bitmask_bytes(ccos_bitmask_t* bitmask);

#endif  // CCOS_DISK_TOOL_CCOS_STRUCTURE_H
