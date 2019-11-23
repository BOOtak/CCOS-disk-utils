#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ccos_image.h>
#include <dumper.h>

typedef enum { MODE_DUMP = 1, MODE_PRINT } op_mode_t;

static const struct option long_options[] = {{"dump-dir", required_argument, NULL, 'd'},
                                             {"print-dir", required_argument, NULL, 'p'},
                                             {"help", no_argument, NULL, 'h'},
                                             {NULL, no_argument, NULL, 0}};

static const char* opt_string = "d:p:h";

static void print_usage() {
  fprintf(stderr,
          "This is a tool for manipulating GRiD OS floppy images.\n"
          "Usage: ccos_disk_tool { -p | -d } <path to GRiD OS floppy RAW image>\n"
          "Options are:\n"
          "\t-p,--print-dir <path> - Open image and print its contents\n"
          "\t-d,--dump-dir <path> - Dump image contents into the current directory\n"
          "\t-h,--help - Show this message\n");
}

int main(int argc, char** argv) {
  op_mode_t mode = 0;
  char* path = NULL;
  int opt = 0;
  while (1) {
    int option_index = 0;
    opt = getopt_long(argc, argv, opt_string, long_options, &option_index);
    if (opt == -1) {
      break;
    }

    switch (opt) {
      case 'd': {
        mode = MODE_DUMP;
        path = optarg;
        break;
      }
      case 'p': {
        mode = MODE_PRINT;
        path = optarg;

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
    default: {
      fprintf(stderr, "Error: no mode selected from { -p | -d }! \n\n");
      print_usage();
      res = -1;
    }
  }

  free(file_contents);
  return res;
}
