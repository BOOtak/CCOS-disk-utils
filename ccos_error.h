#ifndef CCOS_ERROR_H
#define CCOS_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  CCOS_OK = 0,
  CCOS_ENOMEM,   /* Not enough memory (calloc/realloc failed) */
  CCOS_EIO,      /* I/O or internal operation failed */
  CCOS_EINVAL,   /* Invalid argument or format (parse, bitmask, inode state) */
  CCOS_ENOSPC,   /* No space left on image */
  CCOS_EEXIST,   /* File already exists in directory */
  CCOS_ENOENT,   /* No such file or entry in directory */
  CCOS_ERANGE    /* Block or offset out of bounds */
} ccos_error_t;

/**
 * @brief      Return human-readable string for a CCOS error code.
 *
 * @param[in]  err   Error code.
 *
 * @return     Static string describing the error (never NULL).
 */
const char* ccos_error_string(ccos_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* CCOS_ERROR_H */
