#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <ccos_image.h>
#include <common.h>
#include <dumper.h>

typedef enum { MODE_DUMP = 1, MODE_PRINT, MODE_REPLACE_FILE } op_mode_t;

static const struct option long_options[] = {{"image", required_argument, NULL, 'i'},
                                             {"replace-file", required_argument, NULL, 'r'},
                                             {"target-name", required_argument, NULL, 'n'},
                                             {"in-place", no_argument, NULL, 'l'},
                                             {"dump-dir", no_argument, NULL, 'd'},
                                             {"print-contents", no_argument, NULL, 'p'},
                                             {"verbose", no_argument, NULL, 'v'},
                                             {"help", no_argument, NULL, 'h'},
                                             {NULL, no_argument, NULL, 0}};

static const char* opt_string = "i:f:r:n:ldpvh";

static void print_usage() {
  fprintf(stderr,
          "This is a tool for manipulating GRiD OS floppy images.\n"
          "Usage:\n"
          "ccos_disk_tool { -i <image> | -h } [OPTIONS]\n"
          "\n"
          "Options are:\n"
          "{ -f <filename> | -r <file> [-n <name>] [-l] | -d | -p }\n"
          "\n"
          "-i, --image <path>\t\tPath to GRiD OS floppy RAW image\n"
          "-p, --print-contents\t\tPrint image contents\n"
          "-d, --dump-dir\t\t\tDump image contents into the current directory\n"
          "-r, --replace-file <filename>\tReplace file in the image with the given\n"
          "\t\t\t\tfile, save changes to <path>.new\n"
          "-n, --target-name <name>\tOptionally, replace file <name> in the image\n"
          "\t\t\t\tinstead of basename of file passed with\n"
          "\t\t\t\t--replace-file\n"
          "-l, --in-place\t\t\tWrite changes in the original image\n"
          "-v, --verbose\t\t\tVerbose output\n"
          "-h, --help\t\t\tShow this message\n");
}

int main(int argc, char** argv) {
  op_mode_t mode = 0;
  char* path = NULL;
  char* filename = NULL;
  char* target_name = NULL;
  int in_place = 0;
  int opt = 0;
  while (1) {
    int option_index = 0;
    opt = getopt_long(argc, argv, opt_string, long_options, &option_index);
    if (opt == -1) {
      break;
    }

    switch (opt) {
      case 'i': {
        path = optarg;
        break;
      }
      case 'l': {
        in_place = 1;
        break;
      }
      case 'n': {
        target_name = optarg;
        break;
      }
      case 'd': {
        mode = MODE_DUMP;
        break;
      }
      case 'p': {
        mode = MODE_PRINT;
        break;
      }
      case 'r': {
        mode = MODE_REPLACE_FILE;
        filename = optarg;
        break;
      }
      case 'v': {
        trace_init(1);
        break;
      }
      case 'h': {
        print_usage();
        return 0;
      }
    }
  }

  if (path == NULL) {
    fprintf(stderr, "Error: no path to disk image was passed!\n\n");
    print_usage();
    return -1;
  }

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open %s: %s!\n", path, strerror(errno));
    return -1;
  }

  struct stat st = {};
  if (stat(path, &st) == -1) {
    fprintf(stderr, "Unable to stat %s: %s!\n", path, strerror(errno));
    return -1;
  }

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "Unable to open \"%s\": not a file!\n", path);
    return -1;
  }

  long file_size = st.st_size;

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

  TRACE("superblock: 0x%x", superblock);

  int res = -1;
  switch (mode) {
    case MODE_PRINT: {
      res = print_image_info(path, superblock, file_contents);
      break;
    }
    case MODE_DUMP: {
      res = dump_dir(path, superblock, file_contents);
      break;
    }
    case MODE_REPLACE_FILE: {
      res = replace_file(path, filename, target_name, superblock, file_contents, (size_t)file_size, in_place);
      break;
    }
    default: {
      fprintf(stderr, "Error: no mode selected! \n\n");
      print_usage();
      res = -1;
    }
  }

  free(file_contents);
  return res;
}
