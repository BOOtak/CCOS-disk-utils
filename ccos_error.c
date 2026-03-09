#include "ccos_error.h"

const char* ccos_error_string(ccos_error_t err) {
  switch (err) {
    case CCOS_OK:     return "Success";
    case CCOS_ENOMEM: return "Out of memory";
    case CCOS_EIO:    return "I/O error";
    case CCOS_EINVAL: return "Invalid argument or format";
    case CCOS_ENOSPC: return "No space left on image";
    case CCOS_EEXIST: return "File exists";
    case CCOS_ENOENT: return "No such entry";
    case CCOS_ERANGE: return "Out of range";
    default:          return "Unknown error";
  }
}
