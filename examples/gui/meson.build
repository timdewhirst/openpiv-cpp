
# dependencies
imgui_dep = dependency('imgui')
sdl2_dep = dependency('sdl2')
sdl2_image_dep = dependency('sdl2_image')

# source files
app_src = files('main.cpp')

# ImGuiFD files
fd_src = files('ImGuiFD/ImGuiFD.cpp', 'ImGuiFD/ImGuiFDNative.cpp')

# executable
executable(
  'gui',
  sources: app_src + fd_src,
  include_directories: [
    include_directories('../../openpiv')
  ],
  dependencies: [
    pocketfft_dep,
    openpiv_dep,
    cxxopts_dep,
    imgui_dep,
    sdl2_dep,
    sdl2_image_dep,
    threads_dep,
  ],
  cpp_args: [
    '-DIMGUI_ENABLE_SDL2_RENDERER',
  ],
  install: false,
)
