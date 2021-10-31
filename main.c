#include <ccos_image.h>
#include <common.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wrapper.h>

typedef enum {
  MODE_DUMP = 1,
  MODE_PRINT,
  MODE_REPLACE_FILE,
  MODE_COPY_FILE,
  MODE_DELETE_FILE,
  MODE_CREATE_DIRECTORY,
  MODE_ADD_FILE,
  MODE_RENAME_FILE,
  MODE_CREATE_BLANK,
} op_mode_t;

static const struct option long_options[] = {{"image", required_argument, NULL, 'i'},
                                             {"replace-file", required_argument, NULL, 'r'},
                                             {"copy-file", required_argument, NULL, 'c'},
                                             {"rename-file", required_argument, NULL, 'e'},
                                             {"delete-file", required_argument, NULL, 'z'},
                                             {"target-image", required_argument, NULL, 't'},
                                             {"target-name", required_argument, NULL, 'n'},
                                             {"create-dir", required_argument, NULL, 'y'},
                                             {"in-place", no_argument, NULL, 'l'},
                                             {"add-file", required_argument, NULL, 'a'},
                                             {"dump-dir", no_argument, NULL, 'd'},
                                             {"print-contents", no_argument, NULL, 'p'},
                                             {"short-format", no_argument, NULL, 's'},
                                             {"verbose", no_argument, NULL, 'v'},
                                             {"help", no_argument, NULL, 'h'},
                                             {"create-new", no_argument, NULL, 'w'},
                                             {NULL, no_argument, NULL, 0}};

static const char* opt_string = "i:r:n:c:e:a:t:y:z:ldpsvhw";

static void print_usage() {
  fprintf(stderr,
          "This is a tool for manipulating GRiD OS floppy images.\n"
          "Usage:\n"
          "ccos_disk_tool [ -i image | -h ] OPTIONS [-v]\n"
          "\n"
          "Examples:\n"
          "ccos_disk_tool -i image -p [-s]\n"
          "ccos_disk_tool -i image -d\n"
          "ccos_disk_tool -i image -y dir_name\n"
          "ccos_disk_tool -i image -a file -n name [-l]\n"
          "ccos_disk_tool -i src_image -c name -t dest_image [-l]\n"
          "ccos_disk_tool -i src_image -e old name -n new name [-l]\n"
          "ccos_disk_tool -i image -r file -n name [-l]\n"
          "ccos_disk_tool -i image -z name [-l]\n"
          "ccos_disk_tool -i image --create-new\n"
          "\n"
          "-i, --image IMAGE        Path to GRiD OS floppy RAW image\n"
          "-h, --help               Show this message\n"
          "-v, --verbose            Verbose output\n"
          "\n"
          "OPTIONS:\n"
          "-w, --create-new         Create new blank image\n"
          "-p, --print-contents     Print image contents\n"
          "-s, --short-format       Use short format in printing contents\n"
          "                         (80-column compatible, no dates)\n"
          "-d, --dump-dir           Dump image contents into the current directory\n"
          "-a, --add-file FILE      Add file to the image\n"
          "-y, --create-dir NAME    Create new directory\n"
          "-r, --replace-file FILE  Replace file in the image with the given\n"
          "                         file, save changes to IMAGE.out\n"
          "-c, --copy-file NAME     Copy file from one image to another\n"
          "-e, --rename-file FILE   Rename file to the name passed with -n option\n"
          "-t, --target-name FILE   Path to image to copy file to\n"
          "-z, --delete-file FILE   Delete file from the image\n"
          "-n, --target-name NAME   Replace / delete / copy or add file with the name NAME\n"
          "                         in the image\n"
          "-l, --in-place           Write changes to the original image\n");
}

int main(int argc, char** argv) {
  op_mode_t mode = 0;
  char* path = NULL;
  char* filename = NULL;
  char* dir_name = NULL;
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
      case 'e': {
        mode = MODE_RENAME_FILE;
        filename = optarg;
        break;
      }
      case 'a': {
        mode = MODE_ADD_FILE;
        filename = optarg;
        break;
      }
      case 'y': {
        mode = MODE_CREATE_DIRECTORY;
        dir_name = optarg;
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
      case 'w': {
        mode = MODE_CREATE_BLANK;
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

  if (mode == MODE_CREATE_BLANK) {
    return create_blank_image(path);
  }

  uint8_t* file_contents = NULL;
  size_t file_size = 0;
  if (read_file(path, &file_contents, &file_size) == -1) {
    fprintf(stderr, "Unable to read disk image file!\n");
    print_usage();
    return -1;
  }

  if (ccos_check_image(file_contents) == -1) {
    fprintf(stderr, "Unable to get superblock: invalid image format!\n");
    free(file_contents);
    return -1;
  }

  int res;
  switch (mode) {
    case MODE_PRINT: {
      res = print_image_info(path, file_contents, file_size, short_format);
      size_t free_bytes = ccos_calc_free_space(file_contents, file_size);
      printf("\n");
      printf("Free space: " SIZE_T " bytes.\n", free_bytes);
      break;
    }
    case MODE_DUMP: {
      res = dump_image(path, file_contents, file_size);
      break;
    }
    case MODE_REPLACE_FILE: {
      res = replace_file(path, filename, target_name, file_contents, file_size, in_place);
      break;
    }
    case MODE_COPY_FILE: {
      res = copy_file(target_image, filename, file_contents, file_size, in_place);
      break;
    }
    case MODE_DELETE_FILE: {
      res = delete_file(path, filename, in_place);
      break;
    }
    case MODE_ADD_FILE: {
      if (target_name == NULL) {
        fprintf(stderr, "No file name is provided! Usage: -i <image> -a <file path> -n <target name>\n");
        print_usage();
        res = -1;
      } else {
        res = add_file(path, filename, target_name, file_contents, file_size, in_place);
      }
      break;
    }
    case MODE_CREATE_DIRECTORY: {
      res = create_directory(path, dir_name, file_contents, file_size, in_place);
      break;
    }
    case MODE_RENAME_FILE: {
      res = rename_file(path, filename, target_name, file_contents, file_size, in_place);
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
