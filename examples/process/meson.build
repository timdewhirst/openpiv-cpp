
process_src = files('main.cpp')
executable(
  'process',
  sources: process_src,
  include_directories: [
    include_directories('../../openpiv')
  ],
  dependencies: [
    pocketfft_dep,
    openpiv_dep,
    cxxopts_dep,
    threads_dep,
  ],
)
