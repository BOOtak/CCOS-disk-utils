#ifndef CCOS_STRING_H
#define CCOS_STRING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct short_string_t_;
typedef struct short_string_t_ short_string_t;

struct short_string_t_ {
  uint8_t length;
  const char data[0xFF];
};

#ifdef __cplusplus
}
#endif

#endif // CCOS_STRING_H
