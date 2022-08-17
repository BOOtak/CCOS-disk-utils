#include <ccos_image.h>
#include <ccos_private.h>
#include <common.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

uint16_t ccos_file_id(ccos_inode_t* inode) {
  return inode->header.file_id;
}

version_t ccos_get_file_version(ccos_inode_t* file) {
  uint8_t major = file->version_major;
  uint8_t minor = file->version_minor;
  uint8_t patch = file->version_patch;
  version_t version = {major, minor, patch};
  return version;
}

int ccos_set_file_version(ccos_inode_t* file, version_t new_version) {
  if (!is_root_dir(file)) {
    file->version_major = new_version.major;
    file->version_minor = new_version.minor;
    file->version_patch = new_version.patch;
    update_inode_checksums(file);
    return 0;
  }

  return -1;
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

int ccos_set_creation_date(ccos_inode_t* file, ccos_date_t new_date) {
  return change_date(file, new_date, CREATED);
}

int ccos_set_mod_date(ccos_inode_t* file, ccos_date_t new_date) {
  return change_date(file, new_date, MODIF);
}

int ccos_set_exp_date(ccos_inode_t* file, ccos_date_t new_date) {
  return change_date(file, new_date, EXPIR);
}

int ccos_get_dir_contents(ccos_inode_t* dir, uint8_t* data, uint16_t* entry_count, ccos_inode_t*** entries) {
  uint8_t* dir_contents = NULL;
  size_t dir_size = 0;
  if (ccos_read_file(dir, data, &dir_contents, &dir_size) == -1) {
    fprintf(stderr, "Unable to get directory contents: Unable to read directory!\n");
    if (dir_contents != NULL) {
      free(dir_contents);
      return -1;
    }
  }

  parsed_directory_element_t* elements = NULL;
  // TODO: Do we really need entry count here?
  *entry_count = dir->dir_count;
  int res = parse_directory_data(data, dir_contents, dir_size, *entry_count, &elements);
  free(dir_contents);

  if (res == -1) {
    fprintf(stderr, "Unable to get directory contents: Unable to parse directory data!\n");
    if (elements != NULL) {
      free(elements);
      return -1;
    }
  }

  *entries = (ccos_inode_t**)calloc(*entry_count, sizeof(ccos_inode_t*));
  if (*entries == NULL) {
    fprintf(stderr, "Unable to get directory contents: Unable to allocate memory for directory entries: %s!\n",
            strerror(errno));
    free(elements);
    return -1;
  }

  for (int j = 0; j < *entry_count; ++j) {
    (*entries)[j] = (elements)[j].file;
  }

  free(elements);
  return 0;
}

int ccos_is_dir(ccos_inode_t* file) {
  if (is_root_dir(file)) {
    return 1;
  }

  char type[CCOS_MAX_FILE_NAME];
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(file, NULL, type, NULL, NULL);
  if (res == -1) {
    return 0;
  }

  if (strlen(type) != strlen(CCOS_DIR_TYPE)) {
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

char* ccos_get_image_label(uint8_t* data, size_t data_size) {
  ccos_inode_t* root = ccos_get_root_dir(data, data_size);
  char* label = short_string_to_string((short_string_t*)&(root->name_length));
  if (strcmp(label, "")) {
    int sz = strlen(label);
    memmove(label, label + 1, sz - 1);
    label[sz - 1] = 0;
  }
  return label;
}

int ccos_set_image_label(uint8_t* data, size_t data_size, const char* label) {
  char newlab[strlen(label) + 1];
  ccos_inode_t* root = ccos_get_root_dir(data, data_size);
  if (strcmp(label, "")) {
    sprintf(newlab, " %s", label);
    return ccos_rename_file(data, data_size, root, newlab, NULL);
  } else {
    return ccos_rename_file(data, data_size, root, "", NULL);
  }
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
  // correct number is dir_length.
  if (ccos_is_dir(file)) {
    if (file->file_size != file->dir_length) {
      fprintf(stderr, "Warn: dir_length != file_size (%d != %d), fallback to dir_length.\n", file->dir_length,
              file->file_size);
      *file_size = file->dir_length;
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

  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(image_data, image_size);
  if (bitmask_list.length == 0) {
    fprintf(stderr, "Unable to write to file: invalid bitmask!\n");
    return -1;
  }

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
      if (add_block_to_file(file, image_data, &bitmask_list) == CCOS_INVALID_BLOCK) {
        fprintf(stderr, "Unable to allocate more space for the file 0x%x: no space left!\n", file->header.file_id);
        return -1;
      }
    }

    TRACE("Done writing file.");
  } else if (out_blocks_count < blocks_count) {
    TRACE("Removing %d blocks from the file", blocks_count - out_blocks_count);
    for (int i = 0; i < (blocks_count - out_blocks_count); ++i) {
      TRACE("Remove %d / %d...", i + 1, (blocks_count - out_blocks_count));
      if (remove_block_from_file(file, image_data, &bitmask_list) == -1) {
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
  ccos_bitmask_list_t dest_bitmask_list = find_bitmask_blocks(dest_image, dest_image_size);
  if (dest_bitmask_list.length == 0) {
    fprintf(stderr, "Unable to copy file: Unable to get bitmask in destination image!\n");
    return -1;
  }

  uint16_t free_block = CCOS_INVALID_BLOCK;
  if ((free_block = get_free_block(&dest_bitmask_list)) == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to copy file: no space left!\n");
    return -1;
  }

  mark_block(&dest_bitmask_list, free_block, 1);
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
int ccos_delete_file(uint8_t* image_data, size_t data_size, ccos_inode_t* file) {
  if (ccos_is_dir(file)) {
    TRACE("Recursively deleting files in the directory %*s (0x%x)", file->name_length, file->name,
          file->header.file_id);
    uint16_t files = 0;
    ccos_inode_t** content = NULL;
    ccos_get_dir_contents(file, image_data, &files, &content);
    for (int c = 0; c < files; c++) {
      ccos_delete_file(image_data, data_size, content[c]);
    }
    free(content);
  }

  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(image_data, data_size);
  if (bitmask_list.length == 0) {
    fprintf(stderr, "Unable to delete file: Unable to find image bitmask!\n");
    return -1;
  }

  if (delete_file_from_parent_dir(file, image_data, data_size) == -1){
    fprintf(stderr, "Unable to delete file: Unable to delete file entry from parent dir!\n");
    return -1;
  }

  size_t blocks_count = 0;
  uint16_t* blocks = NULL;
  if (get_file_blocks(file, image_data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to read file blocks of file %*s (0x%x)!\n", file->name_length, file->name,
            file->header.file_id);
    return -1;
  }

  for (int j = 0; j < blocks_count; ++j) {
    erase_block(blocks[j], image_data, &bitmask_list);
  }
  free(blocks);

  while (file->content_inode_info.block_next != CCOS_INVALID_BLOCK) {
    if (remove_content_inode(file, image_data, &bitmask_list) == -1) {
      fprintf(stderr, "Unable to remove content block from the file %*s (0x%x)!\n", file->name_length, file->name,
              file->header.file_id);
      return -1;
    }
  }

  erase_block(file->header.file_id, image_data, &bitmask_list);

  return 0;
}

ccos_inode_t* ccos_add_file(ccos_inode_t* dest_directory, uint8_t* file_data, size_t file_size, const char* file_name,
                            uint8_t* image_data, size_t image_size) {
  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(image_data, image_size);
  if (bitmask_list.length == 0) {
    fprintf(stderr, "Unable to add file: Unable to find bitmask in the image!\n");
    return NULL;
  }

  uint16_t free_block = CCOS_INVALID_BLOCK;
  if ((free_block = get_free_block(&bitmask_list)) == CCOS_INVALID_BLOCK) {
    fprintf(stderr, "Unable to get free block: No space left!\n");
    return NULL;
  }

  mark_block(&bitmask_list, free_block, 1);
  ccos_inode_t* new_file = init_inode(free_block, dest_directory->header.file_id, image_data);

  TRACE("Filling file info...");
  new_file->file_size = file_size;
  new_file->dir_file_id = dest_directory->header.file_id;
  new_file->name_length = strlen(file_name);
  strncpy(new_file->name, file_name, strlen(file_name));

  time_t posix_time;
  time(&posix_time);
  struct tm* time_struct;
  time_struct = localtime(&posix_time);

  new_file->creation_date = (ccos_date_t){time_struct->tm_year + 1900,
                                          time_struct->tm_mon + 1,
                                          time_struct->tm_mday,
                                          time_struct->tm_hour,
                                          time_struct->tm_min,
                                          time_struct->tm_sec,
                                          0,
                                          time_struct->tm_wday,
                                          time_struct->tm_yday};
  new_file->mod_date = new_file->creation_date;
  new_file->expiration_date = (ccos_date_t){};

  TRACE("Writing file 0x%lx", new_file->header.file_id);
  if (ccos_write_file(new_file, image_data, image_size, file_data, file_size) == -1) {
    fprintf(stderr, "Unable to write file to file with id 0x%x!\n", new_file->header.file_id);
    return NULL;
  }

  int res = add_file_to_directory(dest_directory, new_file, image_data, image_size);

  if (res == -1) {
    fprintf(stderr, "Unable to copy file: unable to add new file with id 0x%x to the directory with id 0x%x!\n",
            new_file->header.file_id, dest_directory->header.file_id);
    return NULL;
  }

  return new_file;
}

ccos_inode_t* ccos_get_root_dir(uint8_t* data, size_t data_size) {
  uint16_t superblock = 0;

  if (get_superblock(data, data_size, &superblock) == -1) {
    fprintf(stderr, "Unable to get root directory: unable to get superblock!\n");
    return NULL;
  }

  return get_inode(superblock, data);
}

int ccos_validate_file(const ccos_inode_t* file) {
  uint16_t metadata_checksum = calc_inode_metadata_checksum(file);
  if (metadata_checksum != file->metadata_checksum) {
    fprintf(stderr, "Warn: Invalid metadata checksum: expected 0x%hx, got 0x%hx\n", file->metadata_checksum,
            metadata_checksum);
    return -1;
  }

  uint16_t blocks_checksum = calc_inode_blocks_checksum(file);
  if (blocks_checksum != file->content_inode_info.blocks_checksum) {
    fprintf(stderr, "Warn: Invalid block data checksum: expected 0x%hx, got 0x%hx!\n",
            file->content_inode_info.blocks_checksum, blocks_checksum);
    return -1;
  }

  if (file->header.file_id != file->content_inode_info.header.file_id) {
    fprintf(stderr, "Warn: block number mismatch in inode! 0x%hx != 0x%hx\n", file->header.file_id,
            file->content_inode_info.header.file_id);
    return -1;
  }

  return 0;
}

size_t ccos_calc_free_space(uint8_t* data, size_t data_size) {
  uint16_t superblock = 0;
  if (get_superblock(data, data_size, &superblock) == -1) {
    fprintf(stderr, "Unable to calculate free space: Unable to get superblock!\n");
    return -1;
  }

  uint16_t* free_blocks = NULL;
  size_t free_blocks_count = 0;

  ccos_bitmask_list_t bitmask_list = find_bitmask_blocks(data, data_size);
  if (bitmask_list.length == 0) {
    fprintf(stderr, "Unable to calculate free space on the image: Unable to find bitmask!\n");
    return -1;
  }

  if (get_free_blocks(&bitmask_list, data_size, &free_blocks_count, &free_blocks) == -1) {
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

ccos_inode_t* ccos_create_dir(ccos_inode_t* parent_dir, const char* directory_name, uint8_t* image_data,
                              size_t image_size) {
  const char* dir_suffix = "~subject~";

  char* filename = (char*)calloc(strlen(directory_name) + strlen(dir_suffix) + 1, sizeof(char));
  if (filename == NULL) {
    fprintf(stderr, "Unable to create directory: Unable to allocate memory for directory name!\n");
    return NULL;
  }

  sprintf(filename, "%s%s", directory_name, dir_suffix);

  uint8_t directory_contents = CCOS_DIR_LAST_ENTRY_MARKER;
  ccos_inode_t* new_directory =
      ccos_add_file(parent_dir, &directory_contents, DIR_DEFAULT_SIZE, filename, image_data, image_size);
  free(filename);

  if (new_directory == NULL) {
    return NULL;
  }

  // I have no idea what I'm doing. I'm filling different fields of newly created file to match Programs~Subject~ from
  // real images
  new_directory->uses_8087 = 1;
  new_directory->pswd_len = 0xC;
  new_directory->pswd[0] = '\x29';
  new_directory->pswd[1] = '\xFF';
  new_directory->pswd[2] = '\x47';
  new_directory->pswd[3] = '\xC7';

  update_inode_checksums(new_directory);
  return new_directory;
}

int ccos_rename_file(uint8_t* image_data, size_t image_size, ccos_inode_t* file,
                     const char* new_name, const char* new_type) {
  char name[CCOS_MAX_FILE_NAME] = {0};
  char type[CCOS_MAX_FILE_NAME] = {0};

  if (!is_root_dir(file)) {
    if (ccos_parse_file_name(file, name, type, NULL, NULL) == -1) {
      fprintf(stderr, "Unable to rename file: Unable to parse file name!\n");
      return -1;
    }

    ccos_inode_t* parent_dir = ccos_get_parent_dir(file, image_data);

    if (delete_file_from_parent_dir(file, image_data, image_size) == -1){
      fprintf(stderr, "Unable to rename file: Unable to delete old file entry from parent dir!\n");
      return -1;
    }

    memset(file->name, 0, CCOS_MAX_FILE_NAME);

    if (new_type != NULL) {
      snprintf(file->name, CCOS_MAX_FILE_NAME, "%s~%s~", new_name, new_type);
    } else {
      snprintf(file->name, CCOS_MAX_FILE_NAME, "%s~%s~", new_name, type);
    }

    file->name_length = strlen(file->name);

    if (add_file_to_directory(parent_dir, file, image_data, image_size) == -1){
      fprintf(stderr, "Unable to rename file: Unable to add new file entry from parent dir!\n");
      return -1;
    }
  } else {
    memset(file->name, 0, CCOS_MAX_FILE_NAME);
    snprintf(file->name, CCOS_MAX_FILE_NAME, "%s", new_name);
    file->name_length = strlen(file->name);
    update_inode_checksums(file);
  }

  return 0;
}

uint8_t* ccos_create_new_image(size_t* image_size) {
  if (*image_size == 0) {
    *image_size = BLOCK_SIZE * BLOCKS_IN_IMAGE;
  }
  uint8_t* data = (uint8_t*)calloc(*image_size, sizeof(uint8_t));
  if (data == NULL) {
    fprintf(stderr, "Unable to create new image: unable to allocate memory: %s!\n", strerror(errno));
    return NULL;
  }

  if (format_image(data, *image_size) == -1) {
    fprintf(stderr, "Unable to create new image: unable to format image!\n");
    free(data);
    return NULL;
  }

  return data;
}
