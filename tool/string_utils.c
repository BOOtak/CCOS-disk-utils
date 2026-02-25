#include "string_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void replace_char_in_place(char* src, char from, char to) {
  for (int i = 0; i < strlen(src); ++i) {
    if (src[i] == from) {
      src[i] = to;
    }
  }
}

void print_frame(int length) {
  for (int i = 0; i < length; ++i) {
    printf("-");
  }
  printf("\n");
}

const char* trim_string(const char* src, char symbol) {
  int i = 0;
  for (; i < strlen(src) && src[i] == symbol; ++i)
    ;
  return &(src[i]);
}

const char* rtrim_string(const char* src, char symbol) {
  int i = strlen(src);
  for (; i > 0 && src[i] == symbol; --i)
    ;
  return &(src[i]);
}
