# Test working directory (use source directory for now)
test_working_dir = meson.current_source_dir()

test_sources = files(
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
)

test_inc = include_directories('.')

test_names = [
  'enum_helper_test',
  'grid_test',
  'image_algos_test',
  'image_expression_test',
  'image_loader_test',
  'image_stats_test',
  'image_test',
  'image_utils_test',
  'image_view_test',
  'log_test',
  'pixel_types_test',
  'point_test',
  'range_test',
  'rect_test',
  'size_test',
  'stream_utils_test',
  'util_test',
  'vector_field_test',
  'vector_test',
]

foreach i : range(test_names.length())
  test_name = test_names[i]
  test_file = test_sources[i]
  exe = executable(test_name,
    sources: test_file,
    include_directories: [
      test_inc,
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      catch2_dep,
      threads_dep,
    ],
  )
  test(test_name,
    exe,
    args: [],
    workdir: test_working_dir,
    timeout: 30,
  )
endforeach

benchmark_names = ['image_algos_benchmark', 'image_utils_benchmark']
benchmark_sources = files(
  'image_algos_benchmark.cpp',
  'image_utils_benchmark.cpp',
)
foreach i : range(benchmark_names.length())
  bm_name = benchmark_names[i]
  bm_file = benchmark_sources[i]
  exe = executable(bm_name,
    sources: bm_file,
    include_directories: [
      include_directories('../openpiv'),
    ],
    dependencies: [
      openpiv_dep,
      benchmark_dep,
    ],
  )
  benchmark(bm_name, exe)
endforeach
