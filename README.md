![Build Status](https://github.com/timdewhirst/openpiv-cpp/actions/workflows/meson.yml/badge.svg)

# OpenPIV (c++)

An implementation of a PIV analysis engine in C++ using as few dependencies as possible;
the implementation requires a c++23 compliant compiler.

## Build

There are some external dependencies under external/, so when cloning use:

```git clone --recursive <path to git repo>```

Building uses cmake; pre-requisites are:

* a compiler (e.g. `apt install build-essentials`)
* cmake
* git
* (linux) pkg-config (e.g. `apt install pkg-config`)
* curl, zip, unzip, tar (e.g. `apt install curl zip unzip tar`)
* ninja (e.g. `apt install ninja-build`)

To build:

* `cmake -B build -S .`
* `cmake --build build`

To run tests:

* `cd build`
* `ctest`

To change the build type, add `-DCMAKE_BUILD_TYPE` e.g.
`cmake -DCMAKE_BUILD_TYPE=RelWithDebugInfo -B build -S .`.

The binaries are located in the build directory:

* build
  * test -> *_test
  * examples
    * process
    * average_subtract
  * openpiv -> libopenpivcore.so

### Python Bindings

Python bindings are made through pybind11. Building python modules
appears to be a non-trivial subject, however a setup.py is provided to
build the core library as well as the python bindings using cmake.

To build using pip/setuptools:

* create a venv at the same level as the openpivcore directory
* activate the venv
* run pip against the name of the directory containing the openpiv code e.g. `pip install <path to source>`
  * a clean step should happen as part of the build, this can also be run manually with `python setup.py clean`

To build using cmake:

* install pybind11 using pip
* install pytest using pip
* specify the location of the installed pybind11 libs/cmake modules
  using `-Dpybind11_ROOT=$(pybind11-config --cmakedir)`
* specify that the python bindings should be built: `-DBUILD_PYBIND=ON`

e.g.

```
cmake -B build -S . -DBUILD_PYBIND=ON -Dpybind11_ROOT=$(pybind11-config --cmakedir)
```

## Examples

* under build/examples are two simple applications:
  * [process](examples/process/README.md): a straight-forward PIV cross correlator
  * [average_subtract](examples/average_subtract/README.md): a utility to read in n
    images, find the average and write out n new images with the mean subtracted

# TODO

* build
  * [x] travis/github actions/CI
  * [x] add clang/windows/OS X build
  * [ ] python bindings
* core
  * [x] logging
  * [ ] iostream ops for ImageLoaders
  * [ ] image allocator support
    * [ ] shared memory
    * [ ] pooled memory
  * [ ] image loading
    * [x] load/save PNM files
      * [x] save double image
    * [x] support loading multiple images from files: modify loader interface to read header, report number of images, extract indexed images
    * [ ] memory map files - check performance for large files
    * [ ] PNG - lodepng
    * [ ] RAW - libraw looks less than ideal but no alternative?
    * [ ] b16/PCO
  * utilities
    * [x] split RGB to 4xgreyscale
    * [x] join 4xgreyscale to RGB
    * [x] split complex to planes
    * [x] create complex from planes
  * [x] add ImageInterface data iterators
  * [x] remove data() method from ImageInterface, ImageView
  * [x] allow ImageView to be non-const
* algorithms
  * [x] transpose
  * [x] normalize
  * FFT
    * [x] spectral correlation
    * [x] optimize
    * [ ] openCL
    * [ ] apply kernel in Fourier space
    * [x] use SIMD?
    * [x] real -> complex FFT/correlation of real data
  * [ ] direct correlation
  * [x] peak detection
  * [ ] un-bias peak detection - requires information about interrogation area geometry
  * [x] peak fitting
* image processing
  * [ ] change image_view to use array of pointers for each row?
  * [ ] low/high pass filter
* examples
  * [x] parallel cross-correlate
  * [x] image processing
* processing framework
  * [x] cartesian grid generator
  * [ ] further grid generators
  * [ ] median validation with secondary peak check and interpolation
  * [ ] store signal/noise value
  * [ ] processing
  * [ ] marking
  * [ ] iterative analysis
  * [ ] PIV guided PTV?
* data output
  * [ ] output registry
  * [ ] ASCII/CSV
  * [ ] gnuplot/pyplot?
  * [ ] tecplot
* GUI
  * [ ] Qt?
  * [ ] websockets/HTML5?
  * [ ] webassembly + JS/HTML5?
