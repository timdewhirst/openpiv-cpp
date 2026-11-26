
# dependencies
imgui_dep = dependency('imgui')
sdl3_dep = dependency('sdl3')

# source files
app_src = files(
	'main.cpp',
	'backend.cpp',
	'sdl3_backend.cpp'
	)

# ImGuiFD files
fd_src = files(
       'ImGuiFD/ImGuiFD.cpp',
       'ImGuiFD/ImGuiFDNative.cpp'
       )

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
    sdl3_dep,
    threads_dep,
  ],
  install: false,
)
