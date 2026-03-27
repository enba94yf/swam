OUT ?= libwasmmutator.so

all : $(OUT)

# Binaryen dependency configuration.
#
# This project does not vendor Binaryen sources. The build requires:
# - Header: binaryen-c.h
# - Shared library: libbinaryen.so
#
# Configure either:
# - BINARYEN_PATH: path to a Binaryen build directory that contains ./lib/libbinaryen.so
#   and lives under the Binaryen source tree (so ../src/binaryen-c.h exists), or
# - BINARYEN_INC and BINARYEN_LIB: explicit include and library directories.
BINARYEN_PATH   ?=

# Header and library locations. These can be overridden independently.
BINARYEN_INC    ?=
BINARYEN_LIB    ?=

ifeq ($(strip $(BINARYEN_INC)),)
  ifneq ($(strip $(BINARYEN_PATH)),)
    BINARYEN_INC := $(BINARYEN_PATH)/../src
  endif
endif
ifeq ($(strip $(BINARYEN_LIB)),)
  ifneq ($(strip $(BINARYEN_PATH)),)
    BINARYEN_LIB := $(BINARYEN_PATH)/lib
  endif
endif

ifeq ($(strip $(BINARYEN_INC)),)
  $(error Missing Binaryen include path. Set BINARYEN_INC (or set BINARYEN_PATH to a Binaryen build directory).)
endif
ifeq ($(strip $(BINARYEN_LIB)),)
  $(error Missing Binaryen library path. Set BINARYEN_LIB (or set BINARYEN_PATH to a Binaryen build directory).)
endif

ifeq ($(wildcard $(BINARYEN_INC)/binaryen-c.h),)
  $(error Missing Binaryen headers: expected '$(BINARYEN_INC)/binaryen-c.h'. Set BINARYEN_INC (or BINARYEN_PATH) to a valid Binaryen header directory.)
endif
ifeq ($(wildcard $(BINARYEN_LIB)/libbinaryen.so),)
  $(error Missing Binaryen shared library: expected '$(BINARYEN_LIB)/libbinaryen.so'. Set BINARYEN_LIB (or BINARYEN_PATH) to a valid Binaryen lib directory.)
endif

INCLUDE_FLAGS   = -I../../include -I. -I$(BINARYEN_INC)

LDFLAGS	       += -L$(BINARYEN_LIB) -lm -lbinaryen -lstdc++ -Wl,-rpath,$(BINARYEN_LIB)
CFLAGS         += -fPIC -shared -g $(INCLUDE_FLAGS) -DAFL_SWAM

$(OUT): wasm_mutators.c mutator_func.c helper_func.c init_func.c hash_func.c generator_func.c swam_probe.c swam_exploit.c swam_markers.c swam_strip.c swam_scheduler.c swam_profile.c binaryen_wrapper.cpp
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf libwasmmutator.so
