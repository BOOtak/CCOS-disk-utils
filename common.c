#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <common.h>

int trace_silent(FILE* stream, const char* format, ...) {
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

  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "Unable to open \"%s\": not a file!\n", path);
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
    fprintf(stderr, "Unable to allocate %li bytes for the file %s contents: %s!\n", *file_size, path, strerror(errno));
    fclose(f);
    return -1;
  }

  size_t readed = fread(*file_data, sizeof(uint8_t), *file_size, f);
  fclose(f);

  if (readed != *file_size) {
    fprintf(stderr, "Unable to read %li bytes from the file %s: %s!\n", *file_size, path, strerror(errno));
    free(*file_data);
    return -1;
  }

  return 0;
}
