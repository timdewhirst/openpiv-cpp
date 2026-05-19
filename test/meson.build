
# copy data files required for test
test_data = [
    'A_00001_a.tiff',
    'corr_a.tiff',
    'test-mono.tiff',
    'test-rgb.tiff'
]

# Custom target copies files to build dir
foreach f : test_data
  configure_file(input: f, output: f, copy: true)
endforeach

test_sources = [
  'enum_helper_test.cpp',
  'grid_test.cpp',
  'image_algos_test.cpp',
  'image_expression_test.cpp',
  'image_loader_test.cpp',
  'image_stats_test.cpp',
  'image_test.cpp',
  'image_utils_test.cpp',
  'image_view_test.cpp',
  'log_test.cpp',
  'pixel_types_test.cpp',
  'point_test.cpp',
  'range_test.cpp',
  'rect_test.cpp',
  'size_test.cpp',
  'stream_utils_test.cpp',
  'util_test.cpp',
  'vector_field_test.cpp',
  'vector_test.cpp',
]

test_inc = include_directories('.')

foreach i : range(test_sources.length())
  test_file = test_sources[i]
  test_name = test_file.split('.')[0]
  exe = executable(
    test_name,
    sources: test_file,
    include_directories: [
      test_inc,
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      catch2_dep,
      threads_dep
    ],
  )
  test(
    test_name,
    exe,
    workdir: meson.current_build_dir(),
    timeout: 30,
  )
endforeach

benchmark_sources = [
  'image_algos_benchmark.cpp',
  'image_utils_benchmark.cpp',
]
foreach i : range(benchmark_sources.length())
  bm_file = benchmark_sources[i]
  bm_name = bm_file.split('.')[0]
  exe = executable(bm_name,
    sources: bm_file,
    include_directories: [
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      benchmark_dep
    ],
  )
  benchmark(
    bm_name,
    exe,
    workdir: meson.current_build_dir()
  )
endforeach
