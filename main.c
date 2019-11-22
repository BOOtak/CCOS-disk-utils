#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ccos_image.h>

#define VERSION_MAX_SIZE 12  // "255.255.255"

static char* format_version(version_t* version) {
  char* version_string = (char*)calloc(VERSION_MAX_SIZE, sizeof(char));
  if (version_string == NULL) {
    return NULL;
  }

  snprintf(version_string, VERSION_MAX_SIZE, "%u.%u.%u", version->major, version->minor, version->patch);
  return version_string;
}

int print_file_info(uint16_t file_block, const uint8_t* data, int level) {
  const short_string_t* name = ccos_get_file_name(file_block, data);
  char* c_name = ccos_short_string_to_string(name);
  uint32_t file_size = ccos_get_file_size(file_block, data);

  char* basename = strchr(c_name, '~');
  if (basename == NULL) {
    fprintf(stderr, "Invalid name \"%s\": no file type found!\n", c_name);
    free(c_name);
    return -1;
  }

  char* last_char = strrchr(c_name, '~');
  if ((last_char - c_name) != strlen(c_name) - 1) {
    fprintf(stderr, "Invalid name \"%s\": invalid file type format!\n", c_name);
    free(c_name);
    return -1;
  }

  last_char[0] = '\0';

  int basename_length = basename - c_name + 1;
  char* type = basename + 1;

  version_t version = ccos_get_file_version(file_block, data);
  char* version_string = format_version(&version);
  if (version_string == NULL) {
    fprintf(stderr, "Error: invalid file version string!\n");
    free(c_name);
    return -1;
  }

  int formatted_name_length = basename_length + 2 * level;
  char* formatted_name = calloc(formatted_name_length + 1, sizeof(char));
  if (formatted_name == NULL) {
    fprintf(stderr, "Error: unable to allocate memory for formatted name!\n");
    free(version_string);
    free(c_name);
    return -1;
  }

  snprintf(formatted_name, formatted_name_length, "%*.*s", formatted_name_length, basename_length, c_name);

  printf("%-*s%-*s%-*d%s\n", 32, formatted_name, 24, type, 16, file_size, version_string);
  free(c_name);
  free(version_string);
  free(formatted_name);
  return 0;
}

int print_dir_tree(uint16_t block, const uint8_t* data, int level) {
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

  return 0;
}

static void print_frame(int length) {
  for (int i = 0; i < length; ++i) {
    printf("-");
  }
  printf("\n");
}

int print_image_info(const char* path, const uint16_t superblock, const uint8_t* data) {
  char* floppy_name = ccos_short_string_to_string(ccos_get_file_name(superblock, data));

  char* basename = strrchr(path, '/');
  if (basename == NULL) {
    basename = (char*)path;
  } else {
    basename = basename + 1;
  }

  print_frame(strlen(basename) + 2);
  printf("|%s| - ", basename);
  printf("%s\n", floppy_name);
  print_frame(strlen(basename) + 2);
  printf("\n");

  free(floppy_name);

  printf("%-*s%-*s%-*s%s\n", 32, "File name", 24, "File type", 16, "File size", "Version");
  print_frame(80);
  return print_dir_tree(superblock, data, 0);
}

int main(int argc, char const* argv[]) {
  if (argc != 2) {
    printf("Usage: ccos_disk_tool <path to GRiD OS floppy RAW image>\n");
    return -1;
  }

  const char* path = argv[1];
  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open %s: %s!\n", path, strerror(errno));
    return -1;
  }

  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  uint8_t* file_contents = (uint8_t*)calloc(file_size, sizeof(uint8_t));
  if (file_contents == NULL) {
    fprintf(stderr, "Unable to allocate %li bytes for the file %s contents: %s!\n", file_size, path, strerror(errno));
    fclose(f);
    return -1;
  }

  size_t readed = fread(file_contents, sizeof(uint8_t), file_size, f);
  fclose(f);

  if (readed != file_size) {
    fprintf(stderr, "Unable to read %li bytes from the file %s: %s!\n", file_size, path, strerror(errno));
    free(file_contents);
    return -1;
  }

  uint16_t superblock = 0;
  if (ccos_get_superblock(file_contents, file_size, &superblock) == -1) {
    fprintf(stderr, "Unable to get superblock: invalid image format!\n");
    free(file_contents);
    return -1;
  }

  int res = print_image_info(path, superblock, file_contents);

  free(file_contents);
  return res;
}
