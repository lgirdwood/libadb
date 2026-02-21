# libastrodb

Fast General Purpose Astronomical Database.

`libastrodb` provides a fast and efficient way to query and interact with various astronomical catalogs. The project includes a core C library (`astrodb`) and several example applications that demonstrate importing and querying specific astronomical datasets (like NGC, Sky2000, Tycho, GSC, and HyperLeda).

## Dependencies

To build `libastrodb`, you will need the following dependencies:

*   **CMake** (version 3.10 or higher)
*   **C Compiler** (supporting C99 standard, e.g., GCC or Clang)
*   **zlib** (`zlib1g-dev` on Debian/Ubuntu)
*   **libftp** (`ftplib-dev` on Debian/Ubuntu)
*   Standard Math Library (`libm`, usually included with libc)

### Optional Dependencies for Hardware Acceleration

*   **AVX Support**: A CPU and compiler that support Advanced Vector Extensions (`-mavx`).
*   **OpenMP**: A compiler with OpenMP support (for multi-threading).

## Build Targets

The CMake build system defines the following targets:

*   **`astrodb`** (Shared Library): The core library containing catalog parsing, spatial indexing (HTM/KD-Tree), and solving logic.
*   **Examples** (Executables):
    *   `sky2k`: Example for the Sky2000 catalog.
    *   `ngc`: Example for the NGC catalog.
    *   `hyperleda`: Example for the HyperLeda catalog.
    *   `tycho`: Example for the Tycho catalog.
    *   `gsc`: Example for the Guide Star Catalog (see `examples/Readme.md` for specific GSC 1.1 import instructions).

## Build Instructions

`libastrodb` uses CMake for its build system. 

1. **Configure the Project**

   Create a build directory and run CMake. You can enable optional features during this step.

   ```bash
   # Basic configuration
   cmake -B build -S .

   # Configuration with debug symbols and hardware acceleration enabled
   cmake -B build -S . -DENABLE_DEBUG=ON -DENABLE_AVX=ON -DENABLE_OPENMP=ON
   ```

2. **Build the Project**

   Use CMake to compile the library and examples (using multiple CPU cores with `-j`):

   ```bash
   cmake --build build -j
   ```

3. **Install (Optional)**

   To install the `libastrodb` shared library, headers, and `pkg-config` file to your system (e.g., `/usr/local`):

   ```bash
   sudo cmake --install build
   ```

## Usage

After building, you can find the compiled shared library in `build/src/` and the example executables in `build/examples/`.

For instance, to run the NGC example:
```bash
./build/examples/ngc
```
