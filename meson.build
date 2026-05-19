project('openpiv-cpp', 'cpp',
  version: '0.1.0',
  default_options: ['cpp_std=c++23'])

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
catch2_dep = dependency('catch2-with-main')
benchmark_dep = dependency('benchmark_main')
cxxopts_dep = dependency('cxxopts')

# Subdirectories
subdir('openpiv')
subdir('test')

if build_examples
  subdir('examples')
endif
if build_pybind
  subdir('pybind')
endif
