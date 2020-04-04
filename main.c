#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ccos_image.h>
#include <common.h>
#include <dumper.h>

typedef enum { MODE_DUMP = 1, MODE_PRINT, MODE_REPLACE_FILE, MODE_COPY_FILE, MODE_DELETE_FILE } op_mode_t;

static const struct option long_options[] = {{"image", required_argument, NULL, 'i'},
                                             {"replace-file", required_argument, NULL, 'r'},
                                             {"copy-file", required_argument, NULL, 'c'},
                                             {"delete-file", required_argument, NULL, 'z'},
                                             {"target-image", required_argument, NULL, 't'},
                                             {"target-name", required_argument, NULL, 'n'},
                                             {"in-place", no_argument, NULL, 'l'},
                                             {"dump-dir", no_argument, NULL, 'd'},
                                             {"print-contents", no_argument, NULL, 'p'},
                                             {"short-format", no_argument, NULL, 's'},
                                             {"verbose", no_argument, NULL, 'v'},
                                             {"help", no_argument, NULL, 'h'},
                                             {NULL, no_argument, NULL, 0}};

static const char* opt_string = "i:r:n:c:t:z:ldpsvh";

static void print_usage() {
  fprintf(stderr,
          "This is a tool for manipulating GRiD OS floppy images.\n"
          "Usage:\n"
          "ccos_disk_tool { -i <image> | -h } [OPTIONS] [-v]\n"
          "\n"
          "-i, --image <path>\t\tPath to GRiD OS floppy RAW image\n"
          "-h, --help\t\t\tShow this message\n"
          "-v, --verbose\t\t\tVerbose output\n"
          "\n"
          "Options are:\n"
          "-p [-s] | -d | -r <file> [-n <name>] [-l] | -c <file> OPTIONS | -z <file> [-l]\n"
          "\n"
          "-p, --print-contents\t\tPrint image contents\n"
          "-s, --short-format\t\tUse short format in printing contents\n"
          "\t\t\t\t(80-column compatible, no dates)\n"
          "-d, --dump-dir\t\t\tDump image contents into the current directory\n"
          "-r, --replace-file <filename>\tReplace file in the image with the given\n"
          "\t\t\t\tfile, save changes to <path>.new\n"
          "-n, --target-name <name>\tOptionally, replace file <name> in the image\n"
          "\t\t\t\tinstead of basename of file passed with\n"
          "\t\t\t\t--replace-file\n"
          "-c, --copy-file <filename>\tCopy file between images\n"
          "-z, --delete-file <filename>\tDelete file from the image\n"
          "-l, --in-place\t\t\tWrite changes in the original image\n"
          "\n"
          "Copying options are:\n"
          "-t <path> -n <name> [-l]\n"
          "\n"
          "-t, --target-image <filename>\tPath to the image to copy file to\n"
          "-n, --target-name <name>\tName of file to copy\n"
          "-l, --in-place\t\t\tWrite changes in the original image\n"
          );
}

int main(int argc, char** argv) {
  op_mode_t mode = 0;
  char* path = NULL;
  char* filename = NULL;
  char* target_name = NULL;
  char* target_image = NULL;
  int in_place = 0;
  int short_format = 0;
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
      case 's': {
        short_format = 1;
        break;
      }
      case 'r': {
        mode = MODE_REPLACE_FILE;
        filename = optarg;
        break;
      }
      case 'c': {
        mode = MODE_COPY_FILE;
        filename = optarg;
        break;
      }
      case 'z': {
        mode = MODE_DELETE_FILE;
        filename = optarg;
        break;
      }
      case 't': {
        target_image = optarg;
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

  uint8_t* file_contents = NULL;
  size_t file_size = 0;
  if (read_file(path, &file_contents, &file_size) == -1) {
    fprintf(stderr, "Unable to read disk image file!\n");
    print_usage();
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
      res = print_image_info(path, superblock, file_contents, short_format);

      uint16_t* free_blocks = NULL;
      size_t free_blocks_count = 0;
      res |= ccos_get_free_blocks(file_contents, file_size, &free_blocks, &free_blocks_count);

      printf("\n");
      printf("Free space: %ld bytes.\n", free_blocks_count * BLOCK_SIZE);
      free(free_blocks);

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
    case MODE_COPY_FILE: {
      res = copy_file(target_image, filename, superblock, file_contents, (size_t)file_size, in_place);
      break;
    }
    case MODE_DELETE_FILE: {
      res = delete_file(path, filename, superblock, in_place);
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
