# Python bindings using pybind11
pybind_option = get_option('build_pybind')

if pybind_option
  py_mod = import('python')

  # Check for pybind11
  python_dep = dependency('python')
  pybind11_dep = dependency('pybind11')
  if not pybind11_dep.found()
    error('pybind11 not found. Install with: pip install pybind11')
  endif

  py_sources = files(
    'core/module.cpp',
    'core/grid.cpp',
    'core/image.cpp',
    'core/loader.cpp',
    'core/log.cpp',
    'core/pixel_types.cpp',
    'core/point.cpp',
    'core/rect.cpp',
    'core/size.cpp',
    'core/vector.cpp',
  )

  ext_suffix = run_command('python3-config', '--extension-suffix', check: true).stdout().strip().replace('.so', '')
  pybind_mod = shared_library('pyopenpivcore' + ext_suffix,
    sources: py_sources,
    include_directories: [
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      python_dep,
      pybind11_dep,
    ],
    cpp_args: ['-D_USE_MATH_DEFINES'],
    link_args: ['-lpython3.14'],
    name_prefix: '',
    install: true,
  )
endif
