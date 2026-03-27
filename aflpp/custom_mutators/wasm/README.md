# SWAM Wasm Custom Mutator

## Prerequisites

### Install Binaryen

This mutator uses Binaryen's C API and requires:

- `binaryen-c.h`
- `libbinaryen.so`

Install Binaryen using the system package manager or build it from source.

If building from source, a typical configuration is:

```bash
cd /path/to/binaryen
rm -rf build-release && mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIB=OFF -DBUILD_TESTS=OFF ..
cmake --build . -j"$(nproc)"
```

## Configure Environment Variables

Set one of the following:

1) `BINARYEN_PATH` points to the Binaryen build directory. It must contain
`lib/libbinaryen.so`, and the Binaryen source tree must be adjacent so that
`../src/binaryen-c.h` exists.

```bash
export BINARYEN_PATH=/path/to/binaryen/build-release
```

2) `BINARYEN_INC` and `BINARYEN_LIB` explicitly point to the header and library
directories.

Examples:

```bash
export BINARYEN_INC=/usr/include
export BINARYEN_LIB=/usr/lib
```

On some distributions the library directory is multiarch, for example:

```bash
export BINARYEN_LIB=/usr/lib/x86_64-linux-gnu
```

## Build

```bash
make -j"$(nproc)"
```

The output is `libwasmmutator.so`.

## Notes

- The build links with `-Wl,-rpath,$(BINARYEN_LIB)` to locate `libbinaryen.so` at runtime.
- For additional Binaryen dependency details, see `BINARYEN_DEPENDENCY.md`.

