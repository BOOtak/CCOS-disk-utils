#ifndef DUMPER_H
#define DUMPER_H

#include <stdint.h>

int dump_dir(const char* path, const uint16_t superblock, const uint8_t* data);

int print_image_info(const char* path, const uint16_t superblock, const uint8_t* data);

#endif // DUMPER_H
