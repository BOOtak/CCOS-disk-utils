#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ccos_image.h>

void print_file_info(uint16_t file_block, const uint8_t* data, int level) {
  const short_string_t* name = ccos_get_file_name(file_block, data);
  char* c_name = ccos_short_string_to_string(name);
  version_t version = ccos_get_file_version(file_block, data);
  uint32_t file_size = ccos_get_file_size(file_block, data);

  for (int i = 0; i < level; ++i) {
    printf("  ");
  }

  printf("%s, version %d.%d.%d, size %d bytes.\n", c_name, version.major, version.minor, version.patch, file_size);
  free(c_name);
}

int print_dir_tree(uint16_t block, const uint8_t* data, int level) {
  uint16_t files_count = 0;
  uint16_t* root_dir_files = NULL;
  if (ccos_get_dir_contents(block, data, &files_count, &root_dir_files) == -1) {
    fprintf(stderr, "Unable to get root dir contents!\n");
    return -1;
  }

  for (int i = 0; i < files_count; ++i) {
    print_file_info(root_dir_files[i], data, level + 1);
    if (ccos_is_dir(root_dir_files[i], data))
    {
      print_dir_tree(root_dir_files[i], data, level + 1);
    }
  }

  return 0;
}

int print_image_info(const uint8_t* data) {
  uint16_t superblock = ccos_get_superblock(data);
  if (superblock == 0) {
    fprintf(stderr, "Error: invalid superblock value 0x%lx!\n", superblock);
    return -1;
  } else if (superblock != TYPICAL_SUPERBLOCK_1 && superblock != TYPICAL_SUPERBLOCK_2) {
    fprintf(stderr, "Warn: Unusual superblock value 0x%x\n", superblock);
  }

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
  fprintf(stderr, "File size: %li\n", file_size);
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

  int res = print_image_info(file_contents);

  free(file_contents);
  return res;
}
