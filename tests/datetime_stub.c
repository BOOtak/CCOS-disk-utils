#include "ccos_image.h"
#include "ccos_structure.h"

// Stub time for reproducible tests.
ccos_date_t ccos_get_datetime(void) {
  return (ccos_date_t){
    .year = 1985,
    .month = 9,
    .day = 26,
    .hour = 13,
    .minute = 31,
    .second = 1,
    .tenthOfSec = 8,
    .dayOfWeek = 5,
    .dayOfYear = 269,
  };
}
