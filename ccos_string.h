#ifndef CCOS_STRING_H
#define CCOS_STRING_H

#include <stdint.h>

struct short_string_t_;
typedef struct short_string_t_ short_string_t;

struct short_string_t_ {
  uint8_t length;
  const char data[0xFF];
};

/**
 * @brief      Convert the string from the internal short string format into C string.
 *
 * @param[in]  short_string  The short string.
 *
 * @return     Pointer to allocated C string on success, NULL otherwise.
 */
char* short_string_to_string(const short_string_t* short_string);

#endif // CCOS_STRING_H
