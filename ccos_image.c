#include <ccos_image.h>
#include <ccos_private.h>
#include <common.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FAT_MBR_END_OF_SECTOR_MARKER 0xAA55
#define OPCODE_NOP 0x90
#define OPCODE_JMP 0xEB

#define CCOS_DIR_TYPE "subject"

typedef enum { CONTENT_END_MARKER, BLOCK_END_MARKER, END_OF_BLOCK } read_block_status_t;

static int is_fat_image(const uint8_t* data) {
  return ((data[0] == OPCODE_JMP) && (data[2] == OPCODE_NOP) &&
          (*(uint16_t*)&(data[0x1FE]) == FAT_MBR_END_OF_SECTOR_MARKER));
}

static int is_imd_image(const uint8_t* data) {
  return data[0] == 'I' && data[1] == 'M' && data[2] == 'D' && data[3] == ' ';
}

int ccos_check_image(const uint8_t* file_data) {
  if (is_fat_image(file_data)) {
    fprintf(stderr, "FAT floppy image is found; return.\n");
    return -1;
  }

  if (is_imd_image(file_data)) {
    fprintf(stderr,
            "Provided image is in ImageDisk format, please convert it into the raw disk\n"
            "image (.img) before using.\n"
            "\n"
            "(You can use Disk-Utilities from here: https://github.com/keirf/Disk-Utilities)\n");
    return -1;
  }

  return 0;
}

version_t ccos_get_file_version(ccos_inode_t* file) {
  uint8_t major = file->version_major;
  uint8_t minor = file->version_minor;
  uint8_t patch = file->version_patch;
  version_t version = {major, minor, patch};
  return version;
}

short_string_t* ccos_get_file_name(const ccos_inode_t* file) {
  return (short_string_t*)&(file->name_length);
}

uint32_t ccos_get_file_size(ccos_inode_t* file) {
  return file->file_size;
}

ccos_date_t ccos_get_creation_date(ccos_inode_t* file) {
  return file->creation_date;
}

ccos_date_t ccos_get_mod_date(ccos_inode_t* file) {
  return file->mod_date;
}

ccos_date_t ccos_get_exp_date(ccos_inode_t* file) {
  return file->expiration_date;
}

int ccos_get_dir_contents(const ccos_inode_t* dir, uint8_t* data, uint16_t* entry_count, ccos_inode_t*** entries) {
  uint16_t* dir_blocks = NULL;
  size_t blocks_count = 0;

  if (get_file_blocks(dir, data, &blocks_count, &dir_blocks) == -1) {
    return -1;
  }

  uint32_t dir_size = dir->file_size;
  *entry_count = dir->dir_count;

  uint8_t* dir_contents = (uint8_t*)calloc(dir_size, sizeof(uint8_t));
  if (dir_contents == NULL) {
    return -1;
  }

  size_t offset = 0;
  for (size_t i = 0; i < blocks_count; ++i) {
    const uint8_t* start = NULL;
    size_t data_size = 0;
    if (get_block_data(dir_blocks[i], data, &start, &data_size) == -1) {
      free(dir_blocks);
      free(dir_contents);
      return -1;
    }

    memcpy(dir_contents + offset, start, MIN(data_size, dir_size - offset));
    offset += data_size;
  }

  int res = parse_directory_contents(data, dir_contents, dir_size, *entry_count, entries);
  free(dir_contents);
  free(dir_blocks);
  return res;
}

int ccos_is_dir(ccos_inode_t* file) {
  char type[CCOS_MAX_FILE_NAME];
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(file, NULL, type, NULL, NULL);
  if (res == -1) {
    return 0;
  }

  return strncasecmp(type, CCOS_DIR_TYPE, strlen(CCOS_DIR_TYPE)) == 0;
}

int ccos_replace_file(ccos_inode_t* file, const uint8_t* file_data, uint32_t file_size, uint8_t* image_data) {
  uint32_t inode_file_size = ccos_get_file_size(file);
  if (inode_file_size != file_size) {
    fprintf(stderr,
            "Unable to write file: File size mismatch!\n"
            "(size from the block: %d bytes; actual size: %d bytes\n",
            inode_file_size, file_size);
    return -1;
  }

  size_t block_count = 0;
  uint16_t* blocks = NULL;
  if (get_file_blocks(file, image_data, &block_count, &blocks) != 0) {
    fprintf(stderr, "Unable to write file to image: Unable to get file blocks from the block!\n");
    return -1;
  }

  const uint8_t* image_data_part = file_data;
  size_t written_size = 0;
  for (size_t i = 0; i < block_count; ++i) {
    const uint8_t* start = NULL;
    size_t data_size = 0;
    if (get_block_data(blocks[i], image_data, &start, &data_size) != 0) {
      fprintf(stderr, "Unable to write data: Unable to get target block address!\n");
      free(blocks);
      return -1;
    }

    memcpy((uint8_t*)start, image_data_part, MIN(data_size, file_size - written_size));
    image_data_part += data_size;
    written_size += data_size;
  }

  free(blocks);
  return 0;
}

int ccos_get_image_map(const uint8_t* data, size_t data_size, block_type_t** image_map, size_t* free_blocks_count) {
  size_t block_count = data_size / BLOCK_SIZE;
  if (block_count * BLOCK_SIZE != data_size) {
    fprintf(stderr, "Warn: image size (" SIZE_T " bytes) is not a multiple of a block size (%d bytes)\n", data_size,
            BLOCK_SIZE);
  }

  *image_map = (block_type_t*)calloc(block_count, sizeof(block_type_t));
  if (*image_map == NULL) {
    fprintf(stderr, "Unable to allocate memory for " SIZE_T " blocks in block map: %s!\n", block_count,
            strerror(errno));
    return -1;
  }

  *free_blocks_count = 0;
  for (int i = 0; i < block_count; ++i) {
    uint32_t block_header = *(uint32_t*)&(data[i * BLOCK_SIZE]);
    block_type_t block_type = UNKNOWN;
    if (block_header == CCOS_EMPTY_BLOCK_MARKER) {
      *free_blocks_count = *free_blocks_count + 1;
      block_type = EMPTY;
    } else {
      block_type = DATA;
    }

    (*image_map)[i] = block_type;
  }

  return 0;
}

int ccos_read_file(ccos_inode_t* file, const uint8_t* image_data, uint8_t** file_data, size_t* file_size) {
  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (get_file_blocks(file, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file at 0x%x!\n", file->header.file_id);
    return -1;
  }

  *file_size = file->file_size;
  // Probably work-around for compatibility with older CCOS releases?
  // In some cases, inode->dir_length != inode->file_size, e.g. root dir might have file_size = 0x1F8 bytes (maximum
  // size of a file with one content block), and dir_length = 0xD8 (just some number below 0x1F8). In those cases the
  // correct number is dir_length. But sometimes inode->file_size may be > 0x1F8 (e.g file consists of two blocks) and
  // inode->dir_length will be 0xD8. In those cases the correct number is first one.
  if (ccos_is_dir(file)) {
    if (file->file_size != file->dir_length) {
      fprintf(stderr, "Warn: dir_length != file_size (%d != %d)\n", file->dir_length, file->file_size);
      if (file->file_size <= CCOS_BLOCK_DATA_SIZE) {
        fprintf(stderr, "Fallback to dir_length\n");
        *file_size = file->dir_length;
      } else {
        fprintf(stderr, "Use file_size\n");
      }
    }
  }
  uint32_t written = 0;

  *file_data = (uint8_t*)calloc(*file_size, sizeof(uint8_t));
  if (*file_data == NULL) {
    fprintf(stderr, "Unable to allocate " SIZE_T " bytes for file id 0x%x!\n", *file_size, file->header.file_id);
    return -1;
  }

  for (int i = 0; i < blocks_count; ++i) {
    const uint8_t* data_start = NULL;
    size_t data_size = 0;
    if (get_block_data(blocks[i], image_data, &data_start, &data_size) == -1) {
      fprintf(stderr, "Unable to get data for data block 0x%x, file at 0x%x\n", blocks[i], file->header.file_id);
      free(*file_data);
      return -1;
    }

    size_t copy_size = MIN(*file_size - written, data_size);
    memcpy(&((*file_data)[written]), data_start, copy_size);

    written += copy_size;
  }

  if (written != *file_size) {
    fprintf(stderr, "Warn: File size (" SIZE_T ") != amount of bytes read (%u) at file 0x%x!\n", *file_size, written,
            file->header.file_id);
  }

  free(blocks);
  return 0;
}

int ccos_write_file(ccos_inode_t* file, uint8_t* image_data, size_t image_size, const uint8_t* file_data,
                    size_t file_size) {
  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (get_file_blocks(file, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file id 0x%x!\n", file->header.file_id);
    return -1;
  }

  free(blocks);

  ccos_bitmask_t* bitmask = get_bitmask(image_data, image_size);

  TRACE("file id 0x%x has %d blocks", file->header.file_id, blocks_count);
  // add extra blocks to the file if it's new size is greater than the old size
  size_t out_blocks_count = (file_size + CCOS_BLOCK_DATA_SIZE - 1) / CCOS_BLOCK_DATA_SIZE;
  if (out_blocks_count != blocks_count) {
    TRACE("But should contain %d", out_blocks_count);
  }
  if (out_blocks_count > blocks_count) {
    TRACE("Adding %d blocks to the file", out_blocks_count - blocks_count);

    for (int i = 0; i < (out_blocks_count - blocks_count); ++i) {
      TRACE("Adding %d / %d...", i + 1, (out_blocks_count - blocks_count));
      if (add_block_to_file(file, image_data, bitmask) == CCOS_INVALID_BLOCK) {
        fprintf(stderr, "Unable to allocate more space for the file 0x%x: no space left!\n", file->header.file_id);
        return -1;
      }
    }

    TRACE("Done writing file.");
  } else if (out_blocks_count < blocks_count) {
    TRACE("Removing %d blocks from the file", blocks_count - out_blocks_count);
    for (int i = 0; i < (blocks_count - out_blocks_count); ++i) {
      TRACE("Remove %d / %d...", i + 1, (blocks_count - out_blocks_count));
      if (remove_block_from_file(file, image_data, bitmask) == -1) {
        fprintf(stderr, "Unable to remove block from file at 0x%x!\n", file->header.file_id);
        return -1;
      }
    }
  }

  if (get_file_blocks(file, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for the file id 0x%x!\n", file->header.file_id);
    return -1;
  }

  size_t written = 0;
  for (int i = 0; i < blocks_count; ++i) {
    uint8_t* start = NULL;
    size_t data_size = 0;
    get_block_data(blocks[i], image_data, (const uint8_t**)&start, &data_size);

    size_t copy_size = MIN(file_size - written, data_size);
    memcpy(start, &(file_data[written]), copy_size);
    written += copy_size;
  }

  if (written != file_size) {
    fprintf(stderr, "Warn: File size (" SIZE_T ") != amount of bytes read (" SIZE_T ") at file 0x%x!\n", file_size,
            written, file->header.file_id);
  }

  free(blocks);

  if (ccos_is_dir(file)) {
    TRACE("Updating dir_length for %*s as well", file->name_length, file->name);
    file->dir_length = written;
  }
  file->file_size = written;
  update_inode_checksums(file);
  return 0;
}

// allocate block for the new file inode; copy file inode over; write file contents to the new file; add new file to the
// directory
int ccos_copy_file(uint8_t* dest_image, size_t dest_image_size, ccos_inode_t* dest_directory, const uint8_t* src_image,
                   ccos_inode_t* src_file) {
  ccos_bitmask_t* dest_bitmask = get_bitmask(dest_image, dest_image_size);
  if (dest_bitmask == NULL) {
    fprintf(stderr, "Unable to copy file: Unable to find bitmask in destination image!\n");
    return -1;
  }

  uint16_t free_block = CCOS_INVALID_BLOCK;
  if ((free_block = get_free_block(dest_bitmask)) == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to copy file: no space left!\n");
    return -1;
  }

  mark_block(dest_bitmask, free_block, 1);
  ccos_inode_t* new_file = init_inode(free_block, dest_directory->header.file_id, dest_image);

  uint8_t* file_data = NULL;
  size_t file_size = 0;
  TRACE("Reading file 0x%lx (%*s)", src_file->header.file_id, src_file->name_length, src_file->name);
  if (ccos_read_file(src_file, src_image, &file_data, &file_size) == -1) {
    fprintf(stderr, "Unable to read source file with id 0x%x!\n", src_file->header.file_id);
    return -1;
  }

  TRACE("Copying file info over...");
  memcpy(&(new_file->file_size), &(src_file->file_size),
         offsetof(ccos_inode_t, content_inode_info) - offsetof(ccos_inode_t, file_size));

  TRACE("Writing file 0x%lx", new_file->header.file_id);
  if (ccos_write_file(new_file, dest_image, dest_image_size, file_data, file_size) == -1) {
    fprintf(stderr, "Unable to write file to file with id 0x%x!\n", free_block);
    free(file_data);
    return -1;
  }

  int res = add_file_to_directory(dest_directory, new_file, dest_image, dest_image_size);
  free(file_data);

  if (res == -1) {
    fprintf(stderr, "Unable to copy file: unable to add new file with id 0x%x to the directory with id 0x%x!\n",
            new_file->header.file_id, dest_directory->header.file_id);
  }

  return res;
}

// - Find parent directory
//    - Remove filename from its contents
//    - Reduce directory size
//    - Reduce directory entry count
//    - Update directory checksums
// - Find all file blocks; clear them and mark as free
// - Clear all file content inode blocks and mark as free
int ccos_delete_file(uint8_t* data, size_t data_size, ccos_inode_t* file) {
  ccos_inode_t* parent_dir = ccos_get_parent_dir(file, data);

  TRACE("Reading contents of the directory %*s (0x%x)", parent_dir->name_length, parent_dir->name,
        parent_dir->header.file_id);

  size_t dir_size = 0;
  uint8_t* dir_contents = NULL;

  ccos_bitmask_t* bitmask = get_bitmask(data, data_size);
  if (bitmask == NULL) {
    fprintf(stderr, "Unable to delete file: Unable to find image bitmask!\n");
    return -1;
  }

  if (ccos_read_file(parent_dir, data, &dir_contents, &dir_size) == -1) {
    fprintf(stderr, "Unable to read directory contents at directory id 0x%x\n", parent_dir->header.file_id);
    return -1;
  }

  char basename[CCOS_MAX_FILE_NAME] = {0};
  char type[CCOS_MAX_FILE_NAME] = {0};
  size_t basename_length = 0;
  size_t type_length = 0;
  ccos_parse_file_name(file, basename, type, &basename_length, &type_length);

  char entry_name[CCOS_MAX_FILE_NAME] = {0};
  char entry_type[CCOS_MAX_FILE_NAME] = {0};
  size_t entry_name_length = 0;
  size_t entry_type_length = 0;

  int offset = CCOS_DIR_ENTRIES_OFFSET;
  size_t entry_size = 0;
  int parsed_entries = 0;

  uint16_t* prev_dir_entry_suffix = NULL;
  uint16_t* dir_entry_suffix = NULL;

  for (;;) {
    TRACE("Parsing entry #%d...", parsed_entries++);
    dir_entry_t* entry = (dir_entry_t*)&(dir_contents[offset]);
    TRACE("entry block: 0x%x, name length: %d", entry->block, entry->name_length);

    memset(entry_name, 0, CCOS_MAX_FILE_NAME);
    memset(entry_type, 0, CCOS_MAX_FILE_NAME);
    parse_file_name((const short_string_t*)&(entry->name_length), entry_name, entry_type, &entry_name_length,
                    &entry_type_length);
    TRACE("%s (%d)", entry_name, entry_name_length);

    // Compare filename and file type separately
    int res = strncasecmp(entry_name, basename, MIN(entry_name_length, basename_length));
    TRACE("%s %s %s", entry_name, res < 0 ? "<" : res > 0 ? ">" : "==", basename);
    if (res == 0) {
      res = strncasecmp(entry_type, type, MIN(entry_type_length, type_length));
      TRACE("%s %s %s", entry_type, res < 0 ? "<" : res > 0 ? ">" : "==", type);
    }

    prev_dir_entry_suffix = dir_entry_suffix;
    dir_entry_suffix = (uint16_t*)&(dir_contents[offset + sizeof(dir_entry_t) + entry->name_length]);
    TRACE("File suffix: %x", *dir_entry_suffix);
    entry_size = sizeof(dir_entry_t) + entry->name_length + sizeof(uint16_t);

    if (res == 0) {
      TRACE("File is found!");
      break;
    } else if (res < 0) {
      if ((*dir_entry_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
        fprintf(stderr, "Unable to find file \"%*s\" in directory \"%*s\"!\n", file->name_length, file->name,
                parent_dir->name_length, parent_dir->name);
        free(dir_contents);
        return -1;
      }

      offset += entry_size;
      TRACE("Offset = %d", offset);
    } else {
      fprintf(stderr, "Unable to find file \"%*s\" in directory \"%*s\"!\n", file->name_length, file->name,
              parent_dir->name_length, parent_dir->name);
      free(dir_contents);
      return -1;
    }
  }

  // If we remove last entry, mark the one before it as last.
  if ((*dir_entry_suffix & CCOS_DIR_LAST_ENTRY_MARKER) == CCOS_DIR_LAST_ENTRY_MARKER) {
    if (prev_dir_entry_suffix != NULL) {
      *prev_dir_entry_suffix = *prev_dir_entry_suffix | CCOS_DIR_LAST_ENTRY_MARKER;
    }
  }

  memmove(dir_contents + offset, dir_contents + offset + entry_size, dir_size - (offset + entry_size));

  // Zero last bytes at the end of dir contents. It's not necessary if you have last entry marker set correctly, but
  // it'll help read image in HEX editor if removed dir entry will be nice and zeroed.
  memset(dir_contents + dir_size - entry_size, 0, entry_size);
  size_t new_dir_size = dir_size - entry_size;

  // Write dir contents back with old size to overwrite bytes at the end of dir with zeroes.
  int res = ccos_write_file(parent_dir, data, data_size, dir_contents, dir_size);

  // Do that once more with new size to clear freed up content block.
  if (res != -1) {
    res = ccos_write_file(parent_dir, data, data_size, dir_contents, new_dir_size);
  }

  free(dir_contents);
  if (res == -1) {
    fprintf(stderr, "Unable to update directory contents of dir with id=0x%x!\n", parent_dir->header.file_id);
    return -1;
  }

  parent_dir->file_size = new_dir_size;
  parent_dir->dir_length = new_dir_size;
  parent_dir->dir_count = parent_dir->dir_count - 1;

  update_inode_checksums(parent_dir);

  size_t blocks_count = 0;
  uint16_t* blocks = NULL;
  if (get_file_blocks(file, data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to read file blocks of file %*s (0x%x)!\n", file->name_length, file->name,
            file->header.file_id);
    return -1;
  }

  for (int i = 0; i < blocks_count; ++i) {
    erase_block(blocks[i], data, bitmask);
  }
  free(blocks);

  while (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    if (remove_content_inode(file, data, bitmask) == -1) {
      fprintf(stderr, "Unable to remove content block from the file %*s (0x%x)!\n", file->name_length, file->name,
              file->header.file_id);
      return -1;
    }
  }

  erase_block(file->header.file_id, data, bitmask);

  return 0;
}

int ccos_add_file(ccos_inode_t* dest_directory, uint8_t* file_data, size_t file_size, const char* file_name,
                  uint8_t* image_data, size_t image_size) {
  ccos_bitmask_t* bitmask = get_bitmask(image_data, image_size);
  if (bitmask == NULL) {
    fprintf(stderr, "Unable to add file: Unable to find bitmask in the image!\n");
    return -1;
  }

  uint16_t free_block = CCOS_INVALID_BLOCK;
  if ((free_block = get_free_block(bitmask)) == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to get free block: No space left!\n");
    return -1;
  }

  mark_block(bitmask, free_block, 1);
  ccos_inode_t* new_file = init_inode(free_block, dest_directory->header.file_id, image_data);

  TRACE("Filling file info...");
  new_file->file_size = file_size;
  new_file->dir_file_id = dest_directory->header.file_id;
  new_file->name_length = strlen(file_name);
  strncpy(new_file->name, file_name, strlen(file_name));

  new_file->creation_date = dest_directory->creation_date;
  new_file->mod_date = dest_directory->mod_date;
  new_file->expiration_date = dest_directory->expiration_date;

  TRACE("Writing file 0x%lx", new_file->header.file_id);
  if (ccos_write_file(new_file, image_data, image_size, file_data, file_size) == -1) {
    fprintf(stderr, "Unable to write file to file with id 0x%x!\n", new_file->header.file_id);
    return -1;
  }

  int res = add_file_to_directory(dest_directory, new_file, image_data, image_size);

  if (res == -1) {
    fprintf(stderr, "Unable to copy file: unable to add new file with id 0x%x to the directory with id 0x%x!\n",
            new_file->header.file_id, dest_directory->header.file_id);
  }

  return res;
}

ccos_inode_t* ccos_get_root_dir(uint8_t* data, size_t data_size) {
  uint16_t superblock = 0;

  if (get_superblock(data, data_size, &superblock) == -1) {
    fprintf(stderr, "Unable to get root directory: unable to get superblock!\n");
    return NULL;
  }

  return get_inode(superblock, data);
}

void ccos_check_file_checksum(const ccos_inode_t* file) {
  uint16_t metadata_checksum = calc_inode_metadata_checksum(file);
  if (metadata_checksum != file->metadata_checksum) {
    fprintf(stderr, "Warn: Invalid metadata checksum: expected 0x%hx, got 0x%hx\n", file->metadata_checksum,
            metadata_checksum);
  }

  uint16_t blocks_checksum = calc_inode_blocks_checksum(file);
  if (blocks_checksum != file->content_inode_info.blocks_checksum) {
    fprintf(stderr, "Warn: Invalid block data checksum: expected 0x%hx, got 0x%hx!\n",
            file->content_inode_info.blocks_checksum, blocks_checksum);
  }
}

size_t ccos_calc_free_space(uint8_t* data, size_t data_size) {
  uint16_t superblock = 0;
  if (get_superblock(data, data_size, &superblock) == -1) {
    fprintf(stderr, "Unable to calculate free space: Unable to get superblock!\n");
    return -1;
  }

  uint16_t* free_blocks = NULL;
  size_t free_blocks_count = 0;

  ccos_bitmask_t* bitmask = get_bitmask(data, data_size);
  if (bitmask == NULL) {
    fprintf(stderr, "Unable to calculate free space on the image: Unable to find bitmask!\n");
    return -1;
  }

  if (get_free_blocks(bitmask, data_size, &free_blocks_count, &free_blocks) == -1) {
    fprintf(stderr, "Unable to calculate free space: Unable to get free blocks!\n");
    if (free_blocks != NULL) {
      free(free_blocks);
    }

    return -1;
  }

  free(free_blocks);
  return free_blocks_count * BLOCK_SIZE;
}

ccos_inode_t* ccos_get_parent_dir(ccos_inode_t* file, uint8_t* data) {
  uint16_t parent_dir_id = file->dir_file_id;
  return get_inode(parent_dir_id, data);
}

int ccos_parse_file_name(ccos_inode_t* inode, char* basename, char* type, size_t* name_length, size_t* type_length) {
  return parse_file_name((const short_string_t*)&(inode->name_length), basename, type, name_length, type_length);
}
