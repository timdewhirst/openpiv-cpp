
avg_sub_src = files('main.cpp')
executable(
  'average_subtract',
  sources: avg_sub_src,
  include_directories: [
    include_directories('../../openpiv'),
  ],
  dependencies: [
    pocketfft_dep,
    openpiv_dep,
    cxxopts_dep,
    threads_dep,
  ],
)
