# SWAM

This artifact contains the SWAM build of AFL++ and a Wasm custom mutator.

## Dependencies

The Wasm custom mutator requires the Binaryen C API (`binaryen-c.h`,
`libbinaryen.so`).

## Build

```bash
make -C aflpp -j"$(nproc)"

# Binaryen configuration (choose one):
# 1) BINARYEN_PATH=/path/to/binaryen/build-release
# 2) BINARYEN_INC=/path/to/include and BINARYEN_LIB=/path/to/lib
BINARYEN_PATH=/path/to/binaryen/build-release \
  make -C aflpp/custom_mutators/wasm -j"$(nproc)"
```

## Run

In this artifact, `afl-fuzz` only supports `AFL_CUSTOM_MUTATOR_ONLY=1`.

```bash
export AFL_CUSTOM_MUTATOR_LIBRARY="$PWD/aflpp/custom_mutators/wasm/libwasmmutator.so"
export AFL_CUSTOM_MUTATOR_ONLY=1
./aflpp/afl-fuzz -i seeds -o out -- ./target @@
```

More documentation:
- [aflpp/custom_mutators/wasm/README.md](aflpp/custom_mutators/wasm/README.md)
- [aflpp/custom_mutators/wasm/BINARYEN_DEPENDENCY.md](aflpp/custom_mutators/wasm/BINARYEN_DEPENDENCY.md)
