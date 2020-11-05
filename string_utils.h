#ifndef STRING_UTILS_H
#define STRING_UTILS_H

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

/**
 * @brief      Replace all characters in a string.
 *
 * @param      src   The source string.
 * @param[in]  from  The character to replace.
 * @param[in]  to    The character to replace with.
 */
void replace_char_in_place(char* src, char from, char to);

/**
 * @brief      Prints a line of a given length.
 *
 * @param[in]  length  The length of a line to print.
 */
void print_frame(int length);

/**
 * @brief      Trim the string from the left.
 *
 * @param[in]  src     The source string.
 * @param[in]  symbol  The character to trim.
 *
 * @return     Pointer to the trimmed string.
 */
const char* trim_string(const char* src, char symbol);

/**
 * @brief      Trimn the string from the right.
 *
 * @param[in]  src     The source string.
 * @param[in]  symbol  The symbol to trim.
 *
 * @return     Pointer to the trimmed string.
 */
const char* rtrim_string(const char* src, char symbol);

#endif  // STRING_UTILS_H
