#ifndef CCOS_CONTEXT_H
#define CCOS_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct ccos_disk_t_ ccos_disk_t;

typedef struct {
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
} ccos_version_t;

/**
 * @brief      Create a disk handle for a bubble-memory image.
 *
 * @param      data        Image data. Must not be NULL, must point to at least size bytes, and must be free()-able.
 *                         Ownership is transferred to the returned handle on success. On failure, ownership remains
 *                         with the caller.
 * @param[in]  size        Image size in bytes. Must be non-zero and a multiple of 256.
 * @param[in]  superblock  Superblock sector id. Must be non-zero, inside the image, and different from bitmap.
 * @param[in]  bitmap      Bitmap sector id. Must be non-zero, inside the image, and different from superblock.
 *
 * @return     Disk handle on success, NULL otherwise.
 */
ccos_disk_t* ccos_disk_new_bubble(uint8_t* data, size_t size, uint16_t superblock, uint16_t bitmap);

/**
 * @brief      Create a disk handle for an external disk image.
 *
 * @param      data        Image data. Must not be NULL, must point to at least size bytes, and must be free()-able.
 *                         Ownership is transferred to the returned handle on success. On failure, ownership remains
 *                         with the caller.
 * @param[in]  size        Image size in bytes. Must be non-zero and a multiple of 512.
 * @param[in]  superblock  Superblock sector id. Must be non-zero, inside the image, and different from bitmap.
 * @param[in]  bitmap      Bitmap sector id. Must be non-zero, inside the image, and different from superblock.
 *
 * @return     Disk handle on success, NULL otherwise.
 */
ccos_disk_t* ccos_disk_new_extdisk(uint8_t* data, size_t size, uint16_t superblock, uint16_t bitmap);

/**
 * @brief      Free a disk handle and the image data owned by it.
 *
 * @param      disk  Disk handle.
 */
void ccos_disk_free(ccos_disk_t* disk);

uint8_t* ccos_disk_data(ccos_disk_t* disk);
size_t ccos_disk_size(const ccos_disk_t* disk);
uint16_t ccos_disk_sector_size(const ccos_disk_t* disk);
uint16_t ccos_disk_superblock(const ccos_disk_t* disk);
uint16_t ccos_disk_bitmap(const ccos_disk_t* disk);

#ifdef __cplusplus
}
#endif

#endif  // CCOS_CONTEXT_H
