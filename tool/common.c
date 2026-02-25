#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int trace_silent(UNUSED FILE* stream, UNUSED const char* format, ...) {
  return 0;
}

void trace_init(int verbose) {
  if (verbose) {
    trace = fprintf;
  } else {
    trace = trace_silent;
  }
}

int read_file(const char* path, uint8_t** file_data, size_t* file_size) {
  if (path == NULL) {
    fprintf(stderr, "Unable to open file: no path was passed!\n\n");
    return -1;
  }

  struct stat st = {};
  if (stat(path, &st) == -1) {
    fprintf(stderr, "Unable to stat %s: %s!\n", path, strerror(errno));
    return -1;
  }

  FILE* f = fopen(path, "rb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open %s: %s!\n", path, strerror(errno));
    return -1;
  }

  *file_size = st.st_size;

  *file_data = (uint8_t*)calloc(*file_size, sizeof(uint8_t));
  if (*file_data == NULL) {
    fprintf(stderr, "Unable to allocate " SIZE_T " bytes for the file %s contents: %s!\n", *file_size, path,
            strerror(errno));
    fclose(f);
    return -1;
  }

  size_t readed = fread(*file_data, sizeof(uint8_t), *file_size, f);
  fclose(f);

  if (readed != *file_size) {
    fprintf(stderr, "Unable to read " SIZE_T " bytes from the file %s: %s!\n", *file_size, path, strerror(errno));
    free(*file_data);
    return -1;
  }

  return 0;
}

int save_image(const char* source_filename, ccos_disk_t* disk, int in_place) {
  char* dest_filename;
  if (in_place) {
    dest_filename = (char*)source_filename;
  } else {
    const char* out_suffix = ".out";
    dest_filename = (char*)calloc(strlen(source_filename) + strlen(out_suffix) + 1, sizeof(char));
    if (dest_filename == NULL) {
      fprintf(stderr, "Unable to allocate memory for destination file name: %s!\n", strerror(errno));
      return -1;
    }

    sprintf(dest_filename, "%s%s", source_filename, out_suffix);
  }

  FILE* f = fopen(dest_filename, "wb");
  if (f == NULL) {
    fprintf(stderr, "Unable to open \"%s\" for writing: %s!\n", dest_filename, strerror(errno));
  }

  if (!in_place) {
    free(dest_filename);
  }

  if (f == NULL) {
    return -1;
  }

  size_t written = fwrite(disk->data, sizeof(uint8_t), disk->size, f);
  fclose(f);
  if (written != disk->size) {
    fprintf(stderr, "Write size mismatch: Expected " SIZE_T ", but only " SIZE_T " written!\n", disk->size, written);
    return -1;
  }

  return 0;
}

const char* get_basename(const char* path) {
  const char* basename = strrchr(path, '/');

#ifdef _WIN32
  if (basename == NULL) {
    basename = strrchr(path, '\\');
  }
#endif

  if (basename == NULL) {
    basename = path;
  } else {
    basename = basename + 1;
  }

  return basename;
}
