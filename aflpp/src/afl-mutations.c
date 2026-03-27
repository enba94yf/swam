/*
   AFL++ mutation tables
   ---------------------

   This TU provides the single definition of the global mutation tables used
   by afl-mutations.h. All other translation units should include
   "afl-mutations.h" without defining AFL_MUTATIONS_DEFINE_GLOBALS so they
   only see extern declarations.
*/

#include "afl-fuzz.h"

/* Ensure that the global arrays are defined only here. */
#define AFL_MUTATIONS_DEFINE_GLOBALS
#include "afl-mutations.h"

