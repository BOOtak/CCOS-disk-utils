#ifndef STRING_UTILS_H
#define STRING_UTILS_H

void replace_char_in_place(char* src, char from, char to);

void print_frame(int length);

const char* trim_string(const char* src, char symbol);

const char* rtrim_string(const char* src, char symbol);

#endif // STRING_UTILS_H
