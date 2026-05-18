# openpiv library
openpiv_sources = files(
  'core/size.cpp',
  'core/rect.cpp',
  'core/util.cpp',
  'loaders/image_loader.cpp',
  'loaders/pnm_image_loader.cpp',
)

# PocketFFT (header-only, local)
pocketfft_inc = include_directories('../external/pocketfft')

# TIFF (local fallback + system libtiff)
tiff_inc = include_directories('../external/libtiff/4.0.10')

# Use local tif_stream.cxx (always include since it exists in repo)
tiff_sources = files(
  '../external/libtiff/4.0.10/tif_stream.cxx',
  'loaders/tiff_image_loader.cpp',
)

# Create the library
libopenpiv = static_library('openpivcore',
  sources: openpiv_sources + tiff_sources,
  include_directories: [
    include_directories('.'),
    pocketfft_inc,
    tiff_inc,
  ],
  cpp_args: ['-D_USE_MATH_DEFINES'],
  dependencies: [threads_dep, tiff_dep],
  install: false,
)

# Export for use by other modules
openpiv_dep = declare_dependency(
  link_with: libopenpiv,
  include_directories: include_directories('.'),
)
