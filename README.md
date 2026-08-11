![Build Status](https://github.com/timdewhirst/openpiv-cpp/actions/workflows/meson.yml/badge.svg)

# OpenPIV (c++)

An implementation of a PIV analysis engine in C++ using as few dependencies as possible;
the implementation requires a c++23 compliant compiler.

## Build

There are some external dependencies under external/, so when cloning use:

```git clone --recursive <path to git repo>```

Building uses meson; pre-requisites are:

* a compiler (e.g. `apt install build-essentials`)
* meson
* git
* (linux) pkg-config (e.g. `apt install pkg-config`)
* curl, zip, unzip, tar (e.g. `apt install curl zip unzip tar`)
* ninja (e.g. `apt install ninja-build`)

To build:

* `meson setup builddir -Dbuild_pybind=true --reconfigure`
  * add `--buildtype`: debug, debugoptimized, release, minsize
* `meson compile -C builddir`

To run tests:

* `meson test -C builddir`

The binaries are located in the build directory:

* builddir
  * test -> *_test
  * examples
    * process
    * average_subtract
    * ...
  * pybind

### Python Bindings

Python bindings are made through pybind11.

To build using pip:

* create a venv at the same level as the openpivcore directory
* activate the venv
* run pip against the name of the directory containing the openpiv code e.g. `pip install <path to source>`


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
