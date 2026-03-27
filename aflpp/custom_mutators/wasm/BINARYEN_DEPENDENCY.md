# Binaryen Dependency

This custom mutator depends on Binaryen's C API:

- Header: `binaryen-c.h`
- Shared library: `libbinaryen.so`

The build is configured by variables in `Makefile`.

## Supported Layouts

### 1) Binaryen Build Directory Layout

This is the default layout assumed by `BINARYEN_PATH`:

- `$(BINARYEN_PATH)/lib/libbinaryen.so`
- `$(BINARYEN_PATH)/../src/binaryen-c.h`

This matches a typical out-of-tree CMake build under the Binaryen source tree.

Build example:

```bash
cd /path/to/binaryen
rm -rf build-release && mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIB=OFF -DBUILD_TESTS=OFF ..
cmake --build . -j"$(nproc)"
```

Then build the mutator:

```bash
cd /path/to/aflpp/custom_mutators/wasm
make -j"$(nproc)" BINARYEN_PATH=/path/to/binaryen/build-release
```

### 2) Distro Installed Layout

If Binaryen is installed system-wide, the header and library locations may not
match the build-directory layout. In that case, set `BINARYEN_INC` and
`BINARYEN_LIB` directly.

Example:

```bash
cd /path/to/aflpp/custom_mutators/wasm
make -j"$(nproc)" \
  BINARYEN_INC=/usr/include \
  BINARYEN_LIB=/usr/lib
```

Notes:

- The Makefile links with `-Wl,-rpath,$(BINARYEN_LIB)`, so the runtime loader can
  find `libbinaryen.so` without additional environment variables.
- If `libbinaryen.so` is in a standard system library directory, the rpath is
  typically unnecessary; it is kept here for repeatable deployments.

