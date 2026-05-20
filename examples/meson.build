# Examples directory

# Process example
process_src = files('process/main.cpp')
  executable('process',
    sources: process_src,
    include_directories: [
      include_directories('../openpiv')
    ],
    dependencies: [
      pocketfft_dep,
      openpiv_dep,
      cxxopts_dep,
      threads_dep,
    ],
  )

  # Average subtract example
  avg_sub_src = files('average_subtract/main.cpp')
  executable('average_subtract',
    sources: avg_sub_src,
    include_directories: [
      include_directories('../openpiv'),
    ],
    dependencies: [
      pocketfft_dep,
      openpiv_dep,
      cxxopts_dep,
      threads_dep,
    ],
  )
