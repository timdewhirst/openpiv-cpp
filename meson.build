project(
  'openpiv-cpp',
  'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++23']
)

# build specific options
if get_option('buildtype') == 'release'
  add_project_arguments('-ffast-math', language: 'cpp')
endif

# Options
build_examples = get_option('build_examples')
build_pybind = get_option('build_pybind')
ubsan = get_option('ubsan')
asan = get_option('asan')

if ubsan
  add_project_arguments('-fsanitize=undefined', language: 'cpp')
  add_project_link_arguments('-fsanitize=undefined', language: 'cpp')
endif

if asan
  add_project_arguments('-fsanitize=address', language: 'cpp')
  add_project_link_arguments('-fsanitize=address', language: 'cpp')
endif

# dependencies
threads_dep = dependency('threads')
tiff_dep = dependency('libtiff-4')
doctest_dep = dependency('doctest')
benchmark_dep = dependency('benchmark_main')
cxxopts_dep = dependency('cxxopts')
pocketfft_dep = dependency('pocketfft')

# Subdirectories
subdir('openpiv')
subdir('test')

if build_examples
  subdir('examples')
endif
if build_pybind
  subdir('pybind')
endif
