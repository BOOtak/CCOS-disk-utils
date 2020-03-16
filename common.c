#include <stdio.h>

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
