#include "ccos_string.h"

#include <stdlib.h>
#include <string.h>

char* short_string_to_string(const short_string_t* short_string) {
    char* result = calloc(short_string->length + 1, sizeof(char));
    if (result == NULL) {
        return NULL;
    }

    memcpy(result, short_string->data, short_string->length);
    return result;
}
