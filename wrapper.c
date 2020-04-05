#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <ccos_image.h>
#include <common.h>
#include <string_utils.h>
#include <wrapper.h>

#define PROGRAMS_DIR_1 "Programs~Subject~"
#define PROGRAMS_DIR_2 "Programs~subject~"

typedef struct {
  const char* target_name;
  uint16_t target_inode;
} find_file_data_t;

typedef enum { RESULT_OK = 1, RESULT_ERROR, RESULT_BREAK } traverse_callback_result_t;

typedef traverse_callback_result_t (*on_file_t)(uint16_t block, const uint8_t* data, const char* dirname, int level,
                                                void* arg);

typedef traverse_callback_result_t (*on_dir_t)(uint16_t block, const uint8_t* data, const char* dirname, int level,
                                               void* arg);

static char* format_version(version_t* version) {
  char* version_string = (char*)calloc(VERSION_MAX_SIZE, sizeof(char));
  if (version_string == NULL) {
    return NULL;
  }

  snprintf(version_string, VERSION_MAX_SIZE, "%u.%u.%u", version->major, version->minor, version->patch);
  return version_string;
}

static int traverse_ccos_image(uint16_t block, const uint8_t* data, const char* dirname, int level, on_file_t on_file,
                               on_dir_t on_dir, void* arg) {
  uint16_t files_count = 0;
  uint16_t* dir_blocks = NULL;
  if (ccos_get_dir_contents(block, data, &files_count, &dir_blocks) == -1) {
    fprintf(stderr, "Unable to get root dir contents!\n");
    return -1;
  }

  TRACE("Processing %d entries in \"%s\"...", files_count, dirname);

  for (int i = 0; i < files_count; ++i) {
    TRACE("Processing %d/%d...", i + 1, files_count);

    const ccos_inode_t* inode = ccos_get_inode(dir_blocks[i], data);

    if (inode->header.file_id != inode->content_inode_info.header.file_id) {
      fprintf(stderr, "Warn: block number mismatch in inode! 0x%hx != 0x%hx (0x%hx)\n", inode->header.file_id,
              inode->content_inode_info.header.file_id, dir_blocks[i]);
    }

    uint16_t metadata_checksum =
        ccos_make_checksum((const uint8_t*)&(inode->header.file_id), offsetof(ccos_inode_t, metadata_checksum));
    if (metadata_checksum != inode->metadata_checksum) {
      fprintf(stderr, "Warn: Invalid metadata checksum: expected 0x%hx, got 0x%hx\n", inode->metadata_checksum,
              metadata_checksum);
    }

    uint16_t blocks_checksum = ccos_make_inode_blocks_checksum(inode);

    if (blocks_checksum != inode->content_inode_info.blocks_checksum) {
      fprintf(stderr, "Warn: Invalid block data checksum: expected 0x%hx, got 0x%hx!\n",
              inode->content_inode_info.blocks_checksum, blocks_checksum);
    }

    if (ccos_is_dir(dir_blocks[i], data)) {
      TRACE("%d: directory", i + 1);
      char subdir_name[CCOS_MAX_FILE_NAME];
      memset(subdir_name, 0, CCOS_MAX_FILE_NAME);
      if (ccos_parse_file_name(ccos_get_file_name(dir_blocks[i], data), subdir_name, NULL, NULL, NULL) == -1) {
        free(dir_blocks);
        return -1;
      }

      TRACE("%d: Processing directory \"%s\"...", i + 1, subdir_name);

      char* subdir = (char*)calloc(sizeof(char), PATH_MAX);
      if (subdir == NULL) {
        fprintf(stderr, "Unable to allocate memory for subdir!\n");
        free(dir_blocks);
        return -1;
      }

      snprintf(subdir, PATH_MAX, "%s/%s", dirname, subdir_name);

      if (on_dir != NULL) {
        traverse_callback_result_t res;
        if ((res = on_dir(dir_blocks[i], data, dirname, level, arg)) != RESULT_OK) {
          TRACE("on_dir returned %d", res);
          free(dir_blocks);
          free(subdir);
          if (res == RESULT_ERROR) {
            fprintf(stderr, "An error occured, skipping the rest of the image!\n");
            return -1;
          } else {
            return 0;
          }
        }
      }

      int res = traverse_ccos_image(dir_blocks[i], data, subdir, level + 1, on_file, on_dir, arg);
      free(subdir);

      if (res == -1) {
        fprintf(stderr, "An error occured, skipping the rest of the image!\n");
        return -1;
      }
    } else {
      TRACE("%d: file", i + 1);

      if (on_file != NULL) {
        traverse_callback_result_t res;
        if ((res = on_file(dir_blocks[i], data, dirname, level, arg)) != RESULT_OK) {
          TRACE("on_file returned %d", res);
          free(dir_blocks);
          if (res == RESULT_ERROR) {
            fprintf(stderr, "An error occured, skipping the rest of the image!\n");
            return -1;
          } else {
            return 0;
          }
        }
      }
    }
  }

  free(dir_blocks);
  TRACE("\"%s\" traverse complete!", dirname);
  return 0;
}

static traverse_callback_result_t print_file_info(uint16_t file_block, const uint8_t* data, const char* dirname,
                                                  int level, void* arg) {
  int short_format = *(int*)arg;
  const short_string_t* name = ccos_get_file_name(file_block, data);
  uint32_t file_size = ccos_get_file_size(file_block, data);

  char basename[CCOS_MAX_FILE_NAME];
  char type[CCOS_MAX_FILE_NAME];
  memset(basename, 0, CCOS_MAX_FILE_NAME);
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(name, basename, type, NULL, NULL);
  if (res == -1) {
    fprintf(stderr, "Invalid file name!\n");
    return RESULT_ERROR;
  }

  int formatted_name_length = strlen(basename) + 2 * level;
  char* formatted_name = calloc(formatted_name_length + 1, sizeof(char));
  if (formatted_name == NULL) {
    fprintf(stderr, "Error: unable to allocate memory for formatted name!\n");
    return RESULT_ERROR;
  }

  snprintf(formatted_name, formatted_name_length + 1, "%*s", formatted_name_length, basename);

  version_t version = ccos_get_file_version(file_block, data);
  char* version_string = format_version(&version);
  if (version_string == NULL) {
    fprintf(stderr, "Error: invalid file version string!\n");
    free(formatted_name);
    return RESULT_ERROR;
  }

  ccos_date_t creation_date = ccos_get_creation_date(file_block, data);
  char creation_date_string[16];
  snprintf(creation_date_string, 16, "%04d/%02d/%02d", creation_date.year, creation_date.month, creation_date.day);

  ccos_date_t mod_date = ccos_get_mod_date(file_block, data);
  char mod_date_string[16];
  snprintf(mod_date_string, 16, "%04d/%02d/%02d", mod_date.year, mod_date.month, mod_date.day);

  ccos_date_t exp_date = ccos_get_exp_date(file_block, data);
  char exp_date_string[16];
  snprintf(exp_date_string, 16, "%04d/%02d/%02d", exp_date.year, exp_date.month, exp_date.day);

  if (short_format) {
    printf("%-*s%-*s%-*d%-*s\n", 32, formatted_name, 24, type, 14, file_size, 10, version_string);
  } else {
    printf("%-*s%-*s%-*d%-*s%-*s%-*s%-*s\n", 32, formatted_name, 24, type, 14, file_size, 10, version_string, 16,
           creation_date_string, 16, mod_date_string, 16, exp_date_string);
  }

  free(version_string);
  free(formatted_name);
  return RESULT_OK;
}

int print_image_info(const char* path, const uint16_t superblock, const uint8_t* data, int short_format) {
  char* floppy_name = ccos_short_string_to_string(ccos_get_file_name(superblock, data));
  const char* name_trimmed = trim_string(floppy_name, ' ');

  char* basename = strrchr(path, '/');
  if (basename == NULL) {
    basename = (char*)path;
  } else {
    basename = basename + 1;
  }

  print_frame(strlen(basename) + 2);
  printf("|%s| - ", basename);
  if (strlen(name_trimmed) == 0) {
    printf("No description\n");
  } else {
    printf("%s\n", floppy_name);
  }
  print_frame(strlen(basename) + 2);
  printf("\n");

  free(floppy_name);

  if (short_format) {
    printf("%-*s%-*s%-*s%-*s\n", 32, "File name", 24, "File type", 14, "File size", 10, "Version");
    print_frame(80);
  } else {
    printf("%-*s%-*s%-*s%-*s%-*s%-*s%-*s\n", 32, "File name", 24, "File type", 14, "File size", 10, "Version", 16,
           "Creation date", 16, "Mod. date", 16, "Exp. date");
    print_frame(128);
  }

  return traverse_ccos_image(superblock, data, "", 0, print_file_info, print_file_info, &short_format);
}

static traverse_callback_result_t dump_dir_tree_on_file(uint16_t block, const uint8_t* data, const char* dirname,
                                                        int level, void* arg) {
  char* abspath = (char*)calloc(sizeof(char), PATH_MAX);
  if (abspath == NULL) {
    fprintf(stderr, "Unable to allocate memory for the filename!\n");
    return RESULT_ERROR;
  }

  char* file_name = ccos_short_string_to_string(ccos_get_file_name(block, data));
  if (file_name == NULL) {
    fprintf(stderr, "Unable to get filename at block 0x%x\n", block);
    free(abspath);
    return RESULT_ERROR;
  }

  // some files in CCOS may actually have slashes in their names, like GenericSerialXON/XOFF~Printer~
  replace_char_in_place(file_name, '/', '_');
  snprintf(abspath, PATH_MAX, "%s/%s", dirname, file_name);
  free(file_name);

  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (ccos_get_file_blocks(block, data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file %s at block 0x%x!\n", abspath, block);
    free(abspath);
    return RESULT_ERROR;
  }

  TRACE("Writing to \"%s\"...", abspath);

  FILE* f = fopen(abspath, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open file \"%s\": %s!\n", abspath, strerror(errno));
    free(abspath);
    free(blocks);
    return RESULT_ERROR;
  }

  uint32_t file_size = ccos_get_file_size(block, data);
  uint32_t current_size = 0;

  for (int i = 0; i < blocks_count; ++i) {
    const uint8_t* data_start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(blocks[i], data, &data_start, &data_size) == -1) {
      fprintf(stderr, "Unable to get data for data block 0x%x, file block 0x%x\n", blocks[i], block);
      fclose(f);
      free(abspath);
      free(blocks);
      return RESULT_ERROR;
    }

    size_t write_size = MIN(file_size - current_size, data_size);

    if (fwrite(data_start, sizeof(uint8_t), write_size, f) < write_size) {
      fprintf(stderr, "Unable to write data to \"%s\": %s!\n", abspath, strerror(errno));
      free(abspath);
      fclose(f);
      free(blocks);
      return RESULT_ERROR;
    }

    current_size += write_size;

    if ((i + 1) % 10 == 0) {
      TRACE("Writing block %d/%ld: %d/%d bytes written", i + 1, blocks_count, current_size, file_size);
    }
  }

  fclose(f);
  free(blocks);
  free(abspath);

  TRACE("Done! %d/%d bytes writtem", current_size, file_size);

  return RESULT_OK;
}

static traverse_callback_result_t dump_dir_tree_on_dir(uint16_t block, const uint8_t* data, const char* dirname,
                                                       int level, void* arg) {
  char subdir_name[CCOS_MAX_FILE_NAME];
  memset(subdir_name, 0, CCOS_MAX_FILE_NAME);
  if (ccos_parse_file_name(ccos_get_file_name(block, data), subdir_name, NULL, NULL, NULL) == -1) {
    return -1;
  }

  // some directories have '/' in their names, e.g. "GRiD-OS/Windows 113x, 114x v3.1.5D"
  replace_char_in_place(subdir_name, '/', '_');

  char* subdir = (char*)calloc(sizeof(char), PATH_MAX);
  if (subdir == NULL) {
    fprintf(stderr, "Unable to allocate memory for subdir!\n");
    return RESULT_ERROR;
  }

  snprintf(subdir, PATH_MAX, "%s/%s", dirname, subdir_name);

  int res = mkdir(subdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  free(subdir);

  if (res == -1) {
    fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dirname, strerror(errno));
    return RESULT_ERROR;
  }

  return RESULT_OK;
}

int dump_dir(const char* path, const uint16_t dir_inode, const uint8_t* data) {
  char* floppy_name = ccos_short_string_to_string(ccos_get_file_name(dir_inode, data));
  const char* name_trimmed = trim_string(floppy_name, ' ');

  char* basename = strrchr(path, '/');
  if (basename == NULL) {
    basename = (char*)path;
  } else {
    basename = basename + 1;
  }

  char* dirname = (char*)calloc(sizeof(char), PATH_MAX);
  if (dirname == NULL) {
    fprintf(stderr, "Unable to allocate memory for directory name!\n");
    free(floppy_name);
    return -1;
  }

  if (strlen(name_trimmed) == 0) {
    strcpy(dirname, basename);
  } else {
    const char* idx = rtrim_string(name_trimmed, ' ');
    if (idx == NULL) {
      strcpy(dirname, name_trimmed);
    } else {
      strncpy(dirname, name_trimmed, idx - name_trimmed);
    }
  }

  free(floppy_name);

  // some directories have '/' in their names, e.g. "GRiD-OS/Windows 113x, 114x v3.1.5D"
  replace_char_in_place(dirname, '/', '_');

  if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dirname, strerror(errno));
    free(dirname);
    return -1;
  }

  int res = traverse_ccos_image(dir_inode, data, dirname, 0, dump_dir_tree_on_file, dump_dir_tree_on_dir, NULL);
  free(dirname);
  TRACE("Image dump complete!");
  return res;
}

static traverse_callback_result_t find_file_on_file(uint16_t block, const uint8_t* data, const char* dirname, int level,
                                                    void* arg) {
  char* file_name = ccos_short_string_to_string(ccos_get_file_name(block, data));
  if (file_name == NULL) {
    fprintf(stderr, "Unable to get filename at block 0x%x\n", block);
    return RESULT_ERROR;
  }

  find_file_data_t* target_data = (find_file_data_t*)arg;
  replace_char_in_place(file_name, '/', '_');
  if (strcmp(target_data->target_name, file_name) == 0) {
    target_data->target_inode = block;
    free(file_name);
    return RESULT_BREAK;
  }

  free(file_name);
  return RESULT_OK;
}

static int find_filename(const uint16_t superblock, const uint8_t* data, const char* filename, uint16_t* inode) {
  find_file_data_t find_file_data = {.target_name = filename, .target_inode = 0};

  if (traverse_ccos_image(superblock, data, "", 0, find_file_on_file, find_file_on_file, &find_file_data) == -1) {
    fprintf(stderr, "Unable to find file in image due to the error!\n");
    return -1;
  }

  if (find_file_data.target_inode == 0) {
    fprintf(stderr, "Unable to find file %s in image!\n", filename);
    return -1;
  }

  *inode = find_file_data.target_inode;
  return 0;
}

int replace_file(const char* path, const char* filename, const char* target_name, const uint16_t superblock,
                 uint8_t* data, size_t data_size, int in_place) {
  uint16_t inode = 0;
  const char* basename;

  if (target_name != NULL) {
    basename = target_name;
  } else {
    basename = strrchr(filename, '/');
    if (basename == NULL) {
      basename = filename;
    } else {
      basename = basename + 1;
    }
  }

  if (find_filename(superblock, data, basename, &inode) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", basename);
    return -1;
  }

  FILE* target_file = fopen(filename, "rb");
  if (target_file == NULL) {
    fprintf(stderr, "Unable to open %s: %s!\n", filename, strerror(errno));
    return -1;
  }

  fseek(target_file, 0, SEEK_END);
  long file_size = ftell(target_file);
  fseek(target_file, 0, SEEK_SET);

  uint8_t* file_contents = (uint8_t*)calloc(file_size, sizeof(uint8_t));
  if (file_contents == NULL) {
    fprintf(stderr, "Unable to allocate %li bytes for the file %s contents: %s!\n", file_size, filename,
            strerror(errno));
    fclose(target_file);
    return -1;
  }

  size_t readed = fread(file_contents, sizeof(uint8_t), file_size, target_file);
  fclose(target_file);

  if (readed != file_size) {
    fprintf(stderr, "Unable to read %li bytes from the file %s: %s!\n", file_size, filename, strerror(errno));
    free(file_contents);
    return -1;
  }

  if (ccos_replace_file(inode, file_contents, file_size, data) == -1) {
    fprintf(stderr, "Unable to overwrite file %s in the image!\n", filename);
    free(file_contents);
    return -1;
  }

  FILE* output;
  if (in_place) {
    output = fopen(path, "wb");
  } else {
    char output_path[PATH_MAX];
    memset(output_path, 0, PATH_MAX);
    snprintf(output_path, PATH_MAX, "%s.new", path);
    output = fopen(output_path, "wb");
  }

  if (output == NULL) {
    fprintf(stderr, "Unable to open output file for writing: %s!\n", strerror(errno));
    free(file_contents);
    return -1;
  }

  size_t res = fwrite(data, sizeof(uint8_t), data_size, output);
  free(file_contents);
  fclose(output);

  if (res != data_size) {
    fprintf(stderr, "Unable to write new image: written %li, expected %li: %s!\n", res, data_size, strerror(errno));
    return -1;
  }

  return 0;
}

static int do_copy_file(uint8_t* dest_data, size_t dest_size, uint16_t dest_superblock, const uint8_t* source_data,
                        size_t source_size, uint16_t source_superblock, const char* filename) {
  uint16_t source_block = 0;

  if (find_filename(source_superblock, source_data, filename, &source_block) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", filename);
    return -1;
  }

  const ccos_inode_t* source_file = ccos_get_inode(source_block, source_data);
  uint16_t source_parent_dir_id = source_file->dir_file_id;

  char* source_dir_name = ccos_short_string_to_string(ccos_get_file_name(source_parent_dir_id, source_data));
  uint16_t dest_parent_dir_id = 0;

  if (find_filename(dest_superblock, dest_data, source_dir_name, &dest_parent_dir_id) == -1) {
    fprintf(stderr, "Warn: Unable to find directory %s in dest image, will copy to the " PROGRAMS_DIR_1 " instead.\n",
            source_dir_name);
    if (find_filename(dest_superblock, dest_data, PROGRAMS_DIR_1, &dest_parent_dir_id) == -1 &&
        find_filename(dest_superblock, dest_data, PROGRAMS_DIR_2, &dest_parent_dir_id) == -1) {
      fprintf(stderr, "Warn: Unable to find directory %s in dest image, will copy to the root directory instead\n",
              PROGRAMS_DIR_1);
      dest_parent_dir_id = dest_superblock;
    }
  }

  free(source_dir_name);

  ccos_inode_t* dest_directory = ccos_get_inode(dest_parent_dir_id, dest_data);

  return ccos_copy_file(dest_data, dest_size, dest_directory, source_data, source_file);
}

int copy_file(const char* target_image, const char* filename, uint16_t superblock, const uint8_t* source_data,
              size_t source_size, int in_place) {
  if (target_image == NULL) {
    fprintf(stderr, "No target image is provided to copy file to!\n");
    return -1;
  }

  if (filename == NULL) {
    fprintf(stderr, "No file name provided to copy to another image!\n");
    return -1;
  }

  uint8_t* dest_data = NULL;
  size_t dest_size = 0;
  if (read_file(target_image, &dest_data, &dest_size) == -1) {
    fprintf(stderr, "Unable to read target disk image file!\n");
    return -1;
  }

  uint16_t dest_superblock = 0;
  if (ccos_get_superblock(dest_data, dest_size, &dest_superblock) == -1) {
    fprintf(stderr, "Unable to get superblock of the target image!\n");
    free(dest_data);
    return -1;
  }

  if (do_copy_file(dest_data, dest_size, dest_superblock, source_data, source_size, superblock, filename) == -1) {
    fprintf(stderr, "Unable to copy file \"%s\" in \"%s\"!\n", filename, target_image);
    free(dest_data);
    return -1;
  }

  char* dest_filename;
  if (in_place) {
    dest_filename = (char*)target_image;
  } else {
    const char* out_suffix = ".out";
    dest_filename = (char*)calloc(strlen(target_image) + strlen(out_suffix) + 1, sizeof(char));
    if (dest_filename == NULL) {
      fprintf(stderr, "Unable to allocate memory for destination file name: %s!\n", strerror(errno));
      free(dest_data);
      return -1;
    }

    sprintf(dest_filename, "%s%s", target_image, out_suffix);
  }

  FILE* f = fopen(dest_filename, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open \"%s\" to write new image data to: %s!\n", dest_filename, strerror(errno));
    free(dest_data);
  }

  if (!in_place) {
    free(dest_filename);
  }

  if (f == NULL) {
    return -1;
  }

  size_t written = fwrite(dest_data, sizeof(uint8_t), dest_size, f);
  free(dest_data);
  fclose(f);
  if (written != dest_size) {
    fprintf(stderr, "Write size mismatch: Expected %ld, but only %ld written!\n", dest_size, written);
    return -1;
  }

  return 0;
}

int delete_file(const char* path, const char* filename, uint16_t superblock, int in_place) {
  if (path == NULL) {
    fprintf(stderr, "No target image is provided to copy file to!\n");
    return -1;
  }

  if (filename == NULL) {
    fprintf(stderr, "No file name provided to copy to another image!\n");
    return -1;
  }

  uint8_t* data = NULL;
  size_t size = 0;
  if (read_file(path, &data, &size) == -1) {
    fprintf(stderr, "Unable to read target disk image file!\n");
    return -1;
  }

  uint16_t file_block = 0;
  if (find_filename(superblock, data, filename, &file_block) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", filename);
    free(data);
    return -1;
  }

  ccos_inode_t* file = ccos_get_inode(file_block, data);

  if (ccos_delete_file(data, size, file) == -1) {
    fprintf(stderr, "Unable to delete file %s!\n", filename);
  }

  char* dest_filename;
  if (in_place) {
    dest_filename = (char*)path;
  } else {
    const char* out_suffix = ".out";
    dest_filename = (char*)calloc(strlen(path) + strlen(out_suffix) + 1, sizeof(char));
    if (dest_filename == NULL) {
      fprintf(stderr, "Unable to allocate memory for destination file name: %s!\n", strerror(errno));
      free(data);
      return -1;
    }

    sprintf(dest_filename, "%s%s", path, out_suffix);
  }

  FILE* f = fopen(dest_filename, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open \"%s\" to write new image data to: %s!\n", dest_filename, strerror(errno));
    free(data);
  }

  if (!in_place) {
    free(dest_filename);
  }
  if (f == NULL) {
    return -1;
  }

  size_t written = fwrite(data, sizeof(uint8_t), size, f);
  free(data);
  fclose(f);
  if (written != size) {
    fprintf(stderr, "Write size mismatch: Expected %ld, but only %ld written!\n", size, written);
    return -1;
  }

  return 0;
}
