# Python bindings using pybind11
pybind_option = option('build_pybind', type: 'boolean', value: false, description: 'Build Python bindings')

if pybind_option.enabled()
  py_mod = import('python')

  # Check for pybind11
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

  pybind_mod = shared_library('pyopenpivcore',
    sources: py_sources,
    include_directories: [
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      pybind11_dep,
    ],
    cpp_args: ['-D_USE_MATH_DEFINES'],
    install: true,
    install_dir: py_mod.get_install_dir(),
  )

  # Rename for Python (remove lib prefix, add .so/.pyd extension)
  pybind_mod_name = 'pyopenpivcore' + py_mod.ext_suffix
endif
