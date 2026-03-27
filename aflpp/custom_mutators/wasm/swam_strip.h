#ifndef SWAM_STRIP_H
#define SWAM_STRIP_H

#include <binaryen-c.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Walk all functions in the module and replace any global.set instructions
   targeting SWAM marker globals with nop instructions. */
void swam_strip_markers(BinaryenModuleRef mod);

#ifdef __cplusplus
}
#endif

#endif
