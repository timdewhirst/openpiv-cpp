
# dependencies
imgui_dep = dependency('imgui')
sdl2_dep = dependency('sdl2')

# source files
app_src = files('main.cpp')

# executable
executable(
  'gui',
  sources: app_src,
  include_directories: [
    include_directories('../../openpiv')
  ],
  dependencies: [
    pocketfft_dep,
    openpiv_dep,
    cxxopts_dep,
    imgui_dep,
    sdl2_dep,
    threads_dep,
  ],
  cpp_args: [
    '-DIMGUI_ENABLE_SDL2_RENDERER',
  ],
  install: false,
)
