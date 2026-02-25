#include "wrapper.h"
#include "ccos_disk.h"
#include "ccos_format.h"
#include "ccos_image.h"
#include "common.h"
#include "string_utils.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define PROGRAMS_DIR_1 "Programs~Subject~"
#define PROGRAMS_DIR_2 "Programs~subject~"

typedef struct {
  const char* target_name;
  ccos_inode_t* target_file;
} find_file_data_t;

struct stat statbuf;

typedef enum { RESULT_OK = 1, RESULT_ERROR, RESULT_BREAK } traverse_callback_result_t;

typedef traverse_callback_result_t (*on_file_t)(ccos_disk_t* disk, ccos_inode_t* file, const char* dirname, int level, void* arg);

typedef traverse_callback_result_t (*on_dir_t)(ccos_disk_t* disk, ccos_inode_t* dir, const char* dirname, int level, void* arg);

static char* format_version(version_t* version) {
  char* version_string = (char*)calloc(VERSION_MAX_SIZE, sizeof(char));
  if (version_string == NULL) {
    return NULL;
  }

  snprintf(version_string, VERSION_MAX_SIZE, "%u.%u.%u", version->major, version->minor, version->patch);
  return version_string;
}

static int traverse_ccos_image(ccos_disk_t* disk, ccos_inode_t* dir, const char* dirname, int level,
                               on_file_t on_file, on_dir_t on_dir, void* arg) {
  uint16_t files_count = 0;
  ccos_inode_t** dir_contents = NULL;
  if (ccos_get_dir_contents(disk, dir, &files_count, &dir_contents) == -1) {
    fprintf(stderr, "Unable to get root dir contents!\n");
    return -1;
  }

  TRACE("Processing %d entries in \"%s\"...", files_count, dirname);

  for (int i = 0; i < files_count; ++i) {
    TRACE("Processing %d/%d...", i + 1, files_count);

    const ccos_inode_t* inode = dir_contents[i];

    ccos_validate_file(disk, inode);

    if (ccos_is_dir(dir_contents[i])) {
      TRACE("%d: directory", i + 1);
      char subdir_name[CCOS_MAX_FILE_NAME];
      memset(subdir_name, 0, CCOS_MAX_FILE_NAME);
      if (ccos_parse_file_name(dir_contents[i], subdir_name, NULL, NULL, NULL) == -1) {
        free(dir_contents);
        return -1;
      }

      TRACE("%d: Processing directory \"%s\"...", i + 1, subdir_name);

      char* subdir = (char*)calloc(sizeof(char), PATH_MAX);
      if (subdir == NULL) {
        fprintf(stderr, "Unable to allocate memory for subdir!\n");
        free(dir_contents);
        return -1;
      }

      snprintf(subdir, PATH_MAX, "%s/%s", dirname, subdir_name);

      if (on_dir != NULL) {
        traverse_callback_result_t res;
        if ((res = on_dir(disk, dir_contents[i], dirname, level, arg)) != RESULT_OK) {
          TRACE("on_dir returned %d", res);
          free(dir_contents);
          free(subdir);
          if (res == RESULT_ERROR) {
            fprintf(stderr, "An error occurred, skipping the rest of the image!\n");
            return -1;
          } else {
            return 0;
          }
        }
      }

      int res = traverse_ccos_image(disk, dir_contents[i], subdir, level + 1, on_file, on_dir, arg);
      free(subdir);

      if (res == -1) {
        fprintf(stderr, "An error occurred, skipping the rest of the image!\n");
        return -1;
      }
    } else {
      TRACE("%d: file", i + 1);

      if (on_file != NULL) {
        traverse_callback_result_t res;
        if ((res = on_file(disk, dir_contents[i], dirname, level, arg)) != RESULT_OK) {
          TRACE("on_file returned %d", res);
          free(dir_contents);
          if (res == RESULT_ERROR) {
            fprintf(stderr, "An error occurred, skipping the rest of the image!\n");
            return -1;
          } else {
            return 0;
          }
        }
      }
    }
  }

  free(dir_contents);
  TRACE("\"%s\" traverse complete!", dirname);
  return 0;
}

static traverse_callback_result_t print_file_info(
  ccos_disk_t* disk, ccos_inode_t* file, UNUSED const char* dirname, int level, void* arg
) {
  int short_format = *(int*)arg;
  uint32_t file_size = ccos_get_file_size(file);

  char basename[CCOS_MAX_FILE_NAME];
  char type[CCOS_MAX_FILE_NAME];
  memset(basename, 0, CCOS_MAX_FILE_NAME);
  memset(type, 0, CCOS_MAX_FILE_NAME);

  int res = ccos_parse_file_name(file, basename, type, NULL, NULL);
  if (res == -1) {
    fprintf(stderr, "Invalid file name!\n");
    return RESULT_ERROR;
  }

  size_t formatted_name_length = strlen(basename) + 2 * level;
  char* formatted_name = calloc(formatted_name_length + 1, sizeof(char));
  if (formatted_name == NULL) {
    fprintf(stderr, "Error: unable to allocate memory for formatted name!\n");
    return RESULT_ERROR;
  }

  snprintf(formatted_name, formatted_name_length + 1, "%*s", (int)formatted_name_length, basename);

  version_t version = ccos_get_file_version(file);
  char* version_string = format_version(&version);
  if (version_string == NULL) {
    fprintf(stderr, "Error: invalid file version string!\n");
    free(formatted_name);
    return RESULT_ERROR;
  }

  ccos_date_t creation_date = ccos_get_creation_date(file);
  char creation_date_string[16];
  snprintf(creation_date_string, 16, "%04d/%02d/%02d", creation_date.year, creation_date.month, creation_date.day);

  ccos_date_t mod_date = ccos_get_mod_date(file);
  char mod_date_string[16];
  snprintf(mod_date_string, 16, "%04d/%02d/%02d", mod_date.year, mod_date.month, mod_date.day);

  ccos_date_t exp_date = ccos_get_exp_date(file);
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

int print_image_info(ccos_disk_t* disk, const char* path, int short_format) {
  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to print image info: Unable to find root directory!\n");
    return -1;
  }

  char* disk_name = short_string_to_string(ccos_get_file_name(root_dir));
  const char* name_trimmed = trim_string(disk_name, ' ');

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
    printf("%s\n", disk_name);
  }
  print_frame(strlen(basename) + 2);
  printf("\n");

  free(disk_name);

  if (short_format) {
    printf("%-*s%-*s%-*s%-*s\n", 32, "File name", 24, "File type", 14, "File size", 10, "Version");
    print_frame(80);
  } else {
    printf("%-*s%-*s%-*s%-*s%-*s%-*s%-*s\n", 32, "File name", 24, "File type", 14, "File size", 10, "Version", 16,
           "Creation date", 16, "Mod. date", 16, "Exp. date");
    print_frame(128);
  }

  return traverse_ccos_image(disk, root_dir, "", 0, print_file_info, print_file_info, &short_format);
}

static traverse_callback_result_t dump_dir_tree_on_file(
  ccos_disk_t* disk, ccos_inode_t* file, const char* dirname,
  UNUSED int level, UNUSED void* arg
) {
  char* abspath = (char*)calloc(sizeof(char), PATH_MAX);
  if (abspath == NULL) {
    fprintf(stderr, "Unable to allocate memory for the filename!\n");
    return RESULT_ERROR;
  }

  char* file_name = short_string_to_string(ccos_get_file_name(file));
  if (file_name == NULL) {
    fprintf(stderr, "Unable to get filename at file at 0x%x\n", ccos_file_id(file));
    free(abspath);
    return RESULT_ERROR;
  }

  // some files in CCOS may actually have slashes in their names, like GenericSerialXON/XOFF~Printer~
  replace_char_in_place(file_name, '/', '_');
  snprintf(abspath, PATH_MAX, "%s/%s", dirname, file_name);
  free(file_name);

  size_t file_size = 0;
  uint8_t* file_data = NULL;
  if (ccos_read_file(disk, file, &file_data, &file_size) == -1) {
    fprintf(stderr, "Unable to dump file at 0x%x: Unable to get file contents!\n", ccos_file_id(file));
    if (file_data != NULL) {
      free(file_data);
      return RESULT_ERROR;
    }
  }

  TRACE("Writing to \"%s\"...", abspath);

  FILE* f = fopen(abspath, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open file \"%s\": %s!\n", abspath, strerror(errno));
    free(abspath);
    free(file_data);
    return RESULT_ERROR;
  }

  if (fwrite(file_data, sizeof(uint8_t), file_size, f) < file_size) {
    fprintf(stderr, "Unable to write data to \"%s\": %s!\n", abspath, strerror(errno));
    free(abspath);
    free(file_data);
    fclose(f);
    return RESULT_ERROR;
  }

  fclose(f);
  free(file_data);
  free(abspath);

  TRACE("Done!");

  return RESULT_OK;
}

static traverse_callback_result_t dump_dir_tree_on_dir(
  ccos_disk_t* disk, ccos_inode_t* dir, const char* dirname, UNUSED int level, UNUSED void* arg
) {
  char subdir_name[CCOS_MAX_FILE_NAME];
  memset(subdir_name, 0, CCOS_MAX_FILE_NAME);
  if (ccos_parse_file_name(dir, subdir_name, NULL, NULL, NULL) == -1) {
    fprintf(stderr, "Unable to dump directory at 0x%x: Unable to get directory name!\n", ccos_file_id(dir));
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

  int res = MKDIR(subdir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);



  if (res == -1) {
    if (errno == EEXIST) {
      TRACE("Directory \"%s\" already exists! Dumping...", subdir);
    } else {
      fprintf(stderr, "Unable to create directory \"%s\": %s!\n", subdir, strerror(errno));
      free(subdir);
      return RESULT_ERROR;
    }
  }

  free(subdir);
  return RESULT_OK;
}

int dump_image(ccos_disk_t* disk, const char* path) {
  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to dump image: Unable to get root directory!\n");
    return -1;
  }

  return dump_dir(disk, path, root_dir);
}

int dump_file(ccos_disk_t* disk, const char* path_to_dir, ccos_inode_t* file, uint8_t* image_data) {
  char dir_name[CCOS_MAX_FILE_NAME];
  memset(dir_name, 0, CCOS_MAX_FILE_NAME);
  char dir_type[CCOS_MAX_FILE_NAME];
  memset(dir_type, 0, CCOS_MAX_FILE_NAME);
  ccos_parse_file_name(file, dir_name, dir_type, NULL, NULL);

  traverse_callback_result_t res = dump_dir_tree_on_file(disk, file, path_to_dir, 0, NULL);

  if (res == RESULT_ERROR) {
    fprintf(stderr, "Unable to dump file \"%s~%s\": %s!\n", dir_name, dir_type, strerror(errno));
    return -1;
  }

  return 0;
}

int dump_dir(ccos_disk_t* disk, const char* path, ccos_inode_t* dir) {
    char* name_trimmed;
    if (dir == ccos_get_parent_dir(disk, dir)) {
      name_trimmed = strdup(short_string_to_string(ccos_get_file_name(dir)));
      if (strcmp(name_trimmed, "")) {
        int sz = strlen(name_trimmed);
        memmove(name_trimmed, name_trimmed + 1, sz - 1);
        name_trimmed[sz - 1] = 0;
      }
    } else {
      name_trimmed = (char*)calloc(sizeof(char), CCOS_MAX_FILE_NAME);
      char* delim = strchr(ccos_get_file_name(dir)->data, '~');
      strncpy(name_trimmed, ccos_get_file_name(dir)->data, (delim - ccos_get_file_name(dir)->data));
    }

  const char* basename = get_basename(path);

  char* dirname = (char*)calloc(sizeof(char), PATH_MAX);
  if (dirname == NULL) {
    fprintf(stderr, "Unable to allocate memory for directory name!\n");
    free(name_trimmed);
    return -1;
  }

  if (strlen(name_trimmed) == 0) {
    char* dot_position;
    if ((dot_position = strchr(basename, '.')) != NULL) {
      strncpy(dirname, basename, dot_position - basename);
    } else {
      strcpy(dirname, basename);
    }
  } else {
    const char* idx = rtrim_string(name_trimmed, ' ');
    if (idx == NULL) {
      strcpy(dirname, name_trimmed);
    } else {
      strncpy(dirname, name_trimmed, idx - name_trimmed);
    }
  }

  free(name_trimmed);

  // some directories have '/' in their names, e.g. "GRiD-OS/Windows 113x, 114x v3.1.5D"
  replace_char_in_place(dirname, '/', '_');

  if (MKDIR(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      TRACE("Directory \"%s\" already exists! Dumping...", dirname);
    } else {
      fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dirname, strerror(errno));
      free(dirname);
      return -1;
    }
  }

  int res = traverse_ccos_image(disk, dir, dirname, 0, dump_dir_tree_on_file, dump_dir_tree_on_dir, NULL);
  free(dirname);
  TRACE("Image dump complete!");
  return res;
}

int dump_image_to(ccos_disk_t* disk, const char* path, const char* destpath) {
  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to dump image: Unable to get root directory!\n");
    return -1;
  }

  return dump_dir_to(disk, path, root_dir, destpath);
}

int dump_dir_to(ccos_disk_t* disk, const char* path, ccos_inode_t* dir, const char* destpath) {
  char* name_trimmed;
  if (dir == ccos_get_parent_dir(disk, dir)) {
    name_trimmed = strdup(short_string_to_string(ccos_get_file_name(dir)));
    if (strcmp(name_trimmed, "")) {
      int sz = strlen(name_trimmed);
      memmove(name_trimmed, name_trimmed + 1, sz - 1);
      name_trimmed[sz - 1] = 0;
    }
  } else {
    name_trimmed = (char*)calloc(sizeof(char), CCOS_MAX_FILE_NAME);
    char* delim = strchr(ccos_get_file_name(dir)->data, '~');
    strncpy(name_trimmed, ccos_get_file_name(dir)->data, (delim - ccos_get_file_name(dir)->data));
  }

  const char* basename = get_basename(path);

  char* dirname = (char*)calloc(sizeof(char), PATH_MAX);
  if (dirname == NULL) {
    fprintf(stderr, "Unable to allocate memory for directory name!\n");
    free(name_trimmed);
    return -1;
  }

  if (strlen(name_trimmed) == 0) {
    char* dot_position;
    if ((dot_position = strchr(basename, '.')) != NULL) {
      strncpy(dirname, basename, dot_position - basename);
    } else {
      strcpy(dirname, basename);
    }
  } else {
    const char* idx = rtrim_string(name_trimmed, ' ');
    if (idx == NULL) {
      strcpy(dirname, name_trimmed);
    } else {
      strncpy(dirname, name_trimmed, idx - name_trimmed);
    }
  }

  free(name_trimmed);

  // some directories have '/' in their names, e.g. "GRiD-OS/Windows 113x, 114x v3.1.5D"
  replace_char_in_place(dirname, '/', '_');

  char* dest = (char*)calloc(sizeof(char), PATH_MAX);
  if (dirname == NULL) {
    fprintf(stderr, "Unable to allocate memory for directory name!\n");
    return -1;
  }
  strcpy(dest, destpath);
  strcat(dest, "/");
  strcat(dest, dirname);
  free(dirname);

  if (MKDIR(dest, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
    if (errno == EEXIST) {
      TRACE("Directory \"%s\" already exists! Dumping...", dest);
    } else {
      fprintf(stderr, "Unable to create directory \"%s\": %s!\n", dest, strerror(errno));
      free(dest);
      return -1;
    }
  }

  int res = traverse_ccos_image(disk, dir, dest, 0, dump_dir_tree_on_file, dump_dir_tree_on_dir, NULL);
  free(dest);
  TRACE("Image dump complete!");
  return res;
}

static traverse_callback_result_t find_file_on_file(
  ccos_disk_t* disk, ccos_inode_t* file, UNUSED const char* dirname, UNUSED int level, void* arg
) {
  char* file_name = short_string_to_string(ccos_get_file_name(file));
  if (file_name == NULL) {
    fprintf(stderr, "Unable to get filename at 0x%x\n", ccos_file_id(file));
    return RESULT_ERROR;
  }

  find_file_data_t* target_data = (find_file_data_t*)arg;
  replace_char_in_place(file_name, '/', '_');
  if (strcmp(target_data->target_name, file_name) == 0) {
    target_data->target_file = file;
    free(file_name);
    return RESULT_BREAK;
  }

  free(file_name);
  return RESULT_OK;
}

static int find_filename(ccos_disk_t* disk, ccos_inode_t* root_dir, const char* filename, ccos_inode_t** file,
                         int verbose) {
  find_file_data_t find_file_data = {.target_name = filename, .target_file = 0};

  if (traverse_ccos_image(disk, root_dir, "", 0, find_file_on_file, find_file_on_file, &find_file_data) == -1) {
    fprintf(stderr, "Unable to find file in image: Unable to complete search!\n");
    return -1;
  }

  if (find_file_data.target_file == 0) {
    if (verbose) {
      fprintf(stderr, "No file %s in the image.\n", filename);
    }
    return -1;
  }

  *file = find_file_data.target_file;
  return 0;
}

int replace_file(ccos_disk_t* disk, const char* path, const char* filename, const char* target_name, int in_place) {
  const char* basename;

  if (target_name != NULL) {
    basename = target_name;
  } else {
    basename = get_basename(path);
  }

  ccos_inode_t* root_dir = ccos_get_root_dir(disk);

  ccos_inode_t* found_file = NULL;
  if (find_filename(disk, root_dir, basename, &found_file, 1) != 0) {
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

  if (ccos_replace_file(disk, found_file, file_contents, file_size) == -1) {
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

  size_t res = fwrite(disk->data, sizeof(uint8_t), disk->size, output);
  free(file_contents);
  fclose(output);

  if (res != disk->size) {
    fprintf(stderr, "Unable to write new image: written " SIZE_T ", expected " SIZE_T ": %s!\n", res, disk->size,
            strerror(errno));
    return -1;
  }

  return 0;
}

static int do_copy_file(ccos_disk_t* src, ccos_inode_t* src_root_dir, const char* filename,
                        ccos_disk_t* dest, ccos_inode_t* dest_root_dir) {
  ccos_inode_t* source_file = NULL;

  if (find_filename(src, src_root_dir, filename, &source_file, 1) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", filename);
    return -1;
  }

  const ccos_inode_t* source_parent_dir = ccos_get_parent_dir(src, source_file);
  char* source_dir_name = short_string_to_string(ccos_get_file_name(source_parent_dir));

  ccos_inode_t* dest_directory = NULL;

  if (find_filename(dest, dest_root_dir, source_dir_name, &dest_directory, 1) == -1) {
    fprintf(stderr, "Warn: Unable to find directory %s in dest image, will copy to the " PROGRAMS_DIR_1 " instead.\n",
            source_dir_name);
    if (find_filename(dest, dest_root_dir, PROGRAMS_DIR_1, &dest_directory, 0) == -1 &&
        find_filename(dest, dest_root_dir, PROGRAMS_DIR_2, &dest_directory, 0) == -1) {
      fprintf(stderr, "Warn: Unable to find directory %s in dest image, will copy to the root directory instead\n",
              PROGRAMS_DIR_1);
      dest_directory = dest_root_dir;
    }
  }

  free(source_dir_name);

  return ccos_copy_file(src, source_file, dest, dest_directory);
}

int copy_file(ccos_disk_t* src, const char* target_image, const char* filename, int in_place) {
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

  // TODO: Allow to copy file to disk with different properties.
  ccos_disk_t dest = (ccos_disk_t) {
    .sector_size = src->sector_size,
    .superblock_fid = src->superblock_fid,
    .bitmap_fid = src->bitmap_fid,
    .data = dest_data,
    .size = dest_size,
  };

  ccos_inode_t* root_dir;
  if ((root_dir = ccos_get_root_dir(src)) == NULL) {
    fprintf(stderr, "Unable to get root directory of the source image!\n");
    free(dest.data);
    return -1;
  }

  ccos_inode_t* dest_root_dir;
  if ((dest_root_dir = ccos_get_root_dir(&dest)) == NULL) {
    fprintf(stderr, "Unable to get root directory of the target image!\n");
    free(dest.data);
    return -1;
  }

  if (do_copy_file(src,  root_dir, filename, &dest, dest_root_dir) == -1) {
    fprintf(stderr, "Unable to copy file \"%s\" in \"%s\"!\n", filename, target_image);
    free(dest.data);
    return -1;
  }

  int res = save_image(target_image, &dest, in_place);
  free(dest.data);
  return res;
}

int add_file(ccos_disk_t* disk, const char* image_path, const char* file_path, const char* file_name, int in_place) {
  if (image_path == NULL) {
    fprintf(stderr, "No path to image is provided to copy file to!\n");
    return -1;
  }

  if (file_path == NULL) {
    fprintf(stderr, "No file name provided to add to image!\n");
    return -1;
  }

  uint8_t* file_data = NULL;
  size_t file_size = 0;
  if (read_file(file_path, &file_data, &file_size) == -1) {
    fprintf(stderr, "Unable to read target disk image file!\n");
    return -1;
  }

  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to add file to image: Unable to get root directory!\n");
    free(file_data);
    return -1;
  }

  ccos_inode_t* dest_dir = NULL;

  if (find_filename(disk, root_dir, PROGRAMS_DIR_1, &dest_dir, 0) == -1 &&
      find_filename(disk, root_dir, PROGRAMS_DIR_2, &dest_dir, 0) == -1) {
    fprintf(stderr, "Warn: Unable to find directory %s in dest image, will add file to the root directory instead\n",
            PROGRAMS_DIR_1);
    dest_dir = root_dir;
  }

  ccos_inode_t* new_file = ccos_add_file(disk, dest_dir, file_data, file_size, file_name);
  free(file_data);
  if (new_file == NULL) {
    fprintf(stderr, "Unable to copy %s to %s!\n", file_name, file_path);
    return -1;
  }

  return save_image(image_path, disk, in_place);
}

int delete_file(ccos_disk_t* disk, const char* path, const char* filename, int in_place) {
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

  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  ccos_inode_t* file = NULL;
  if (find_filename(disk, root_dir, filename, &file, 1) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", filename);
    free(data);
    return -1;
  }

  if (ccos_delete_file(disk, file) == -1) {
    fprintf(stderr, "Unable to delete file %s!\n", filename);
    free(data);
    return -1;
  }

  int res = save_image(path, disk, in_place);
  free(data);
  return res;
}

int create_directory(ccos_disk_t* disk, char* path, char* directory_name, int in_place) {
  if (path == NULL) {
    fprintf(stderr, "No target image is provided to copy file to!\n");
    return -1;
  }

  if (directory_name == NULL) {
    fprintf(stderr, "No file name provided to copy to another image!\n");
    return -1;
  }

  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to add file to image: Unable to get root directory!\n");
    return -1;
  }

  if (ccos_create_dir(disk, root_dir, directory_name) == NULL) {
    fprintf(stderr, "Unable to create directory!\n");
    return -1;
  }

  return save_image(path, disk, in_place);
}

int rename_file(ccos_disk_t* disk, char* path, char* file_name, char* new_name, int in_place) {
  if (path == NULL) {
    fprintf(stderr, "No target image is provided to copy file to!\n");
    return -1;
  }

  if (file_name == NULL) {
    fprintf(stderr, "No file provided to rename!\n");
    return -1;
  }

  if (new_name == NULL) {
    fprintf(stderr, "No new file name provided to rename file to!\n");
    return -1;
  }

  ccos_inode_t* root_dir = ccos_get_root_dir(disk);
  if (root_dir == NULL) {
    fprintf(stderr, "Unable to rename file: Unable to get root directory!\n");
    return -1;
  }

  ccos_inode_t* file = NULL;
  if (find_filename(disk, root_dir, file_name, &file, 1) != 0) {
    fprintf(stderr, "Unable to find file %s in the image!\n", file_name);
    return -1;
  }

  if (ccos_rename_file(disk, file, new_name, NULL) == -1) {
    char* old_file_name = short_string_to_string(ccos_get_file_name(file));
    fprintf(stderr, "Unable to rename file %s to %s!\n", old_file_name, new_name);
    free(old_file_name);
    return -1;
  }

  int res = save_image(path, disk, in_place);
  return res;
}

int create_blank_image(ccos_disk_t* disk, char* path, size_t size) {
  if (path == NULL) {
    fprintf(stderr, "No target image is provided to copy file to!\n");
    return EINVAL;
  }

  if (size % disk->sector_size != 0) {
    fprintf(stderr, "Image size must be a multiple of the sector size %d\n", disk->sector_size);
    return EINVAL;
  }

  disk_format_t format = disk->sector_size == 256 ? CCOS_DISK_FORMAT_BUBMEM : CCOS_DISK_FORMAT_COMPASS;
  int res = ccos_new_disk_image(format, size, disk);
  if (res) {
    fprintf(stderr, "Failed to create new disk image. Error code: %s\n", strerror(res));
    return res;
  }

  res = save_image(path, disk, 1);
  free(disk->data);
  return res;
}
