#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <ccos_image.h>
#include <dumper.h>
#include <string_utils.h>

#define VERSION_MAX_SIZE 12  // "255.255.255"

#define MIN(A, B) A < B ? A : B

static char* format_version(version_t* version) {
  char* version_string = (char*)calloc(VERSION_MAX_SIZE, sizeof(char));
  if (version_string == NULL) {
    return NULL;
  }

  snprintf(version_string, VERSION_MAX_SIZE, "%u.%u.%u", version->major, version->minor, version->patch);
  return version_string;
}

static int print_file_info(uint16_t file_block, const uint8_t* data, int level) {
  const short_string_t* name = ccos_get_file_name(file_block, data);
  uint32_t file_size = ccos_get_file_size(file_block, data);

  char basename[CCOS_MAX_FILE_NAME];
  char type[CCOS_MAX_FILE_NAME];
  memset(basename, 0, CCOS_MAX_FILE_NAME);
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(name, basename, type);
  if (res == -1) {
    fprintf(stderr, "Invalid file name!\n");
    return -1;
  }

  int formatted_name_length = strlen(basename) + 2 * level;
  char* formatted_name = calloc(formatted_name_length + 1, sizeof(char));
  if (formatted_name == NULL) {
    fprintf(stderr, "Error: unable to allocate memory for formatted name!\n");
    return -1;
  }

  snprintf(formatted_name, formatted_name_length + 1, "%*s", formatted_name_length, basename);

  version_t version = ccos_get_file_version(file_block, data);
  char* version_string = format_version(&version);
  if (version_string == NULL) {
    fprintf(stderr, "Error: invalid file version string!\n");
    free(formatted_name);
    return -1;
  }

  printf("%-*s%-*s%-*d%s\n", 32, formatted_name, 24, type, 16, file_size, version_string);
  free(version_string);
  free(formatted_name);
  return 0;
}

static int print_dir_tree(uint16_t block, const uint8_t* data, int level) {
  uint16_t files_count = 0;
  uint16_t* root_dir_files = NULL;
  if (ccos_get_dir_contents(block, data, &files_count, &root_dir_files) == -1) {
    fprintf(stderr, "Unable to get root dir contents!\n");
    return -1;
  }

  for (int i = 0; i < files_count; ++i) {
    if (print_file_info(root_dir_files[i], data, level) == -1) {
      fprintf(stderr, "An error occured, skipping the rest of the image!\n");
      return -1;
    }

    if (ccos_is_dir(root_dir_files[i], data)) {
      print_dir_tree(root_dir_files[i], data, level + 1);
    }
  }

  free(root_dir_files);
  return 0;
}

int print_image_info(const char* path, const uint16_t superblock, const uint8_t* data) {
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

  printf("%-*s%-*s%-*s%s\n", 32, "File name", 24, "File type", 16, "File size", "Version");
  print_frame(80);
  return print_dir_tree(superblock, data, 0);
}

static int dump_file(uint16_t block, const uint8_t* data, const char* dirname) {
  char* abspath = (char*)calloc(sizeof(char), PATH_MAX);
  if (abspath == NULL) {
    fprintf(stderr, "Unable to allocate memory for the filename!\n");
    return -1;
  }

  char* file_name = ccos_short_string_to_string(ccos_get_file_name(block, data));
  if (file_name == NULL) {
    fprintf(stderr, "Unable to get filename at block 0x%lx\n", block);
    free(abspath);
    return -1;
  }

  // some files in CCOS may actually have slashes in their names, like GenericSerialXON/XOFF~Printer~
  replace_char_in_place(file_name, '/', '_');
  snprintf(abspath, PATH_MAX, "%s/%s", dirname, file_name);
  free(file_name);

  size_t blocks_count = 0;
  uint16_t* blocks = NULL;

  if (ccos_get_file_blocks(block, data, &blocks_count, &blocks) == -1) {
    fprintf(stderr, "Unable to get file blocks for file %s at block 0x%lx!\n", abspath, block);
    free(abspath);
    return -1;
  }

  FILE* f = fopen(abspath, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open file \"%s\": %s!\n", abspath, strerror(errno));
    free(abspath);
    free(blocks);
    return -1;
  }

  uint32_t file_size = ccos_get_file_size(block, data);
  uint32_t current_size = 0;

  for (int i = 0; i < blocks_count; ++i) {
    const uint8_t* data_start = NULL;
    size_t data_size = 0;
    if (ccos_get_block_data(blocks[i], data, &data_start, &data_size) == -1) {
      fprintf(stderr, "Unable to get data for data block 0x%lx, file block 0x%lx\n", blocks[i], block);
      fclose(f);
      free(abspath);
      free(blocks);
      return -1;
    }

    size_t write_size = MIN(file_size - current_size, data_size);

    if (fwrite(data_start, sizeof(uint8_t), write_size, f) < write_size) {
      fprintf(stderr, "Unable to write data to \"%s\": %s!\n", abspath, strerror(errno));
      free(abspath);
      fclose(f);
      free(blocks);
      return -1;
    }

    current_size += write_size;
  }

  fclose(f);
  free(blocks);
  free(abspath);
  return 0;
}

static int dump_dir_tree(const uint16_t block, const uint8_t* data, const char* dirname) {
  uint16_t files_count = 0;
  uint16_t* root_dir_files = NULL;

  if (ccos_get_dir_contents(block, data, &files_count, &root_dir_files) == -1) {
    fprintf(stderr, "Unable to get root dir contents!\n");
    return -1;
  }

  for (int i = 0; i < files_count; ++i) {
    if (ccos_is_dir(root_dir_files[i], data)) {
      char subdir_name[CCOS_MAX_FILE_NAME];
      memset(subdir_name, 0, CCOS_MAX_FILE_NAME);
      if (ccos_parse_file_name(ccos_get_file_name(root_dir_files[i], data), subdir_name, NULL) == -1) {
        free(root_dir_files);
        return -1;
      }

      char* subdir = (char*)calloc(sizeof(char), PATH_MAX);
      if (subdir == NULL) {
        fprintf(stderr, "Unable to allocate memory for subdir!\n");
        free(root_dir_files);
        return -1;
      }

      snprintf(subdir, PATH_MAX, "%s/%s", dirname, subdir_name);

      if (mkdir(subdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dirname, strerror(errno));
        free(subdir);
        free(root_dir_files);
        return -1;
      }

      dump_dir_tree(root_dir_files[i], data, subdir);
      free(subdir);
    } else if (dump_file(root_dir_files[i], data, dirname) == -1) {
      fprintf(stderr, "An error occured, skipping the rest of the image!\n");
      return -1;
    }
  }

  free(root_dir_files);
  return 0;
}

int dump_dir(const char* path, const uint16_t superblock, const uint8_t* data) {
  char* floppy_name = ccos_short_string_to_string(ccos_get_file_name(superblock, data));
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

  if (mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dirname, strerror(errno));
    free(dirname);
    return -1;
  }

  int res = dump_dir_tree(superblock, data, dirname);
  free(dirname);
  return res;
}
