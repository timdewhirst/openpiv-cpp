project(
  'openpiv-cpp',
  'cpp',
  version: '0.1.0',
)

# request C++23 in a compiler-agnostic way: MSVC does not accept
# cpp_std=c++23 and requires the dialect as a plain argument
cpp = meson.get_compiler('cpp')
add_project_arguments(
  cpp.get_supported_arguments(['-std=c++23', '/std:c++latest']),
  language: 'cpp',
)

# build specific options
if get_option('buildtype') == 'release'
  if meson.get_compiler('cpp').get_id() == 'msvc'
    add_project_arguments('/fp:fast', language: 'cpp')
  else
    add_project_arguments('-ffast-math', language: 'cpp')
  endif
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
# mimalloc's allocator override has corrupted the heap intermittently
# on macOS arm64; CI disables it there via -Duse_mimalloc=disabled and
# the system allocator is used instead
mimalloc_dep = dependency('mimalloc', required: get_option('use_mimalloc'))

# Subdirectories
subdir('openpiv')
subdir('test')

if build_examples
  subdir('examples')
endif
if build_pybind
  subdir('pybind')
endif
