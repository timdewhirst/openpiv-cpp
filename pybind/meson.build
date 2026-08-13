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

  # get info
  ext_suffix = run_command(
    'python3',
    '-c',
    'import sysconfig; print(sysconfig.get_config_var("EXT_SUFFIX"))',
    check: true).stdout().strip().replace('.so', '')
  message('extension: ', ext_suffix)
  py_lib_name = run_command(
    'python3',
    '-c',
    'import sys; print(f"python{sys.version_info.major}.{sys.version_info.minor}")',
    check: true).stdout().strip()
  message('py_lib_name: ', py_lib_name)
  py_lib_dir = run_command(
    'python3',
    '-c',
    'import sysconfig; print(sysconfig.get_path("platstdlib"))',
    check: true).stdout().strip()
  message('py_lib_dir: ', py_lib_dir)
  py_site_dir = run_command(
    'python3',
    '-c',
    'import sysconfig; print(sysconfig.get_path("platlib"))',
    check: true).stdout().strip()
  message('install directory: ', py_site_dir)

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
    link_args: ['-l' + py_lib_name],
    name_prefix: '',
    install: true,
    install_dir: py_site_dir,
  )
endif
