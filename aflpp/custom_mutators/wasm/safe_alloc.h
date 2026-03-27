#ifndef SAFE_ALLOC_H
#define SAFE_ALLOC_H

#include <stdlib.h>
#include <stddef.h>

/* Safe realloc wrapper that preserves the original pointer on failure.
   Returns 1 on success, 0 on failure.
   On failure, the original pointer remains valid and unchanged. */
static inline int safe_realloc(void** ptr, size_t new_size) {
    void* new_ptr = realloc(*ptr, new_size);
    if (new_ptr == NULL && new_size > 0) {
        /* Allocation failed - original pointer is still valid */
        return 0;
    }
    *ptr = new_ptr;
    return 1;
}

/* Convenience macro for common pattern: realloc array and increment counter */
#define SAFE_REALLOC_ARRAY(ptr, count, type) \
    safe_realloc((void**)&(ptr), sizeof(type) * ((count) + 1))

#endif /* SAFE_ALLOC_H */
