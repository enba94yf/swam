#ifndef SWAM_MARKERS_H
#define SWAM_MARKERS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int swam_markers_init(void);

void swam_markers_read(uint32_t* before, uint32_t* after, bool* valid);

void swam_markers_reset(void);

void swam_markers_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif  // SWAM_MARKERS_H
