#
# unit tests:
#

# include catch
find_package(Catch2 REQUIRED)
find_package(Threads REQUIRED)

# ensure we make a test dir in build as a working directory
set(TEST_WORKING_DIR "${CMAKE_BINARY_DIR}/test_working_dir")
file(MAKE_DIRECTORY ${TEST_WORKING_DIR})

function(create_copy_target TARGET_NAME SOURCE_FILES DEST_DIR)
  file(GLOB FILES_TO_COPY ${SOURCE_FILES})
  if(FILES_TO_COPY)
    add_custom_target(${TARGET_NAME}
      COMMAND ${CMAKE_COMMAND} -E make_directory ${DEST_DIR}
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${FILES_TO_COPY}
      "${DEST_DIR}/"
      COMMENT "Copying test files to ${DEST_DIR}"
      VERBATIM
    )
  endif()
endfunction()
create_copy_target(
  copy_image_files
  "${CMAKE_SOURCE_DIR}/test/*.tiff"
  ${TEST_WORKING_DIR})

file(GLOB SOURCES *_test.cpp)
foreach(SOURCE ${SOURCES})
  get_filename_component(OUTPUT ${SOURCE} NAME_WE)
  message("building ${OUTPUT} from ${SOURCE}")

  # for each file, build a binary
  add_executable(${OUTPUT} ${SOURCE})
  add_dependencies(${OUTPUT} copy_image_files)

  # include openpivcore
  include_directories(${CMAKE_SOURCE_DIR}/openpiv)

  target_link_libraries(
    ${OUTPUT}
    PRIVATE Catch2::Catch2WithMain
    PRIVATE openpivcore
    PRIVATE Threads::Threads)

  # add as a test
  add_test(
    NAME ${OUTPUT}
    COMMAND ${OUTPUT}
    WORKING_DIRECTORY ${TEST_WORKING_DIR})
endforeach()

#
# benchmark tests:
#

# include google benchmark
find_package(benchmark)

if(benchmark_FOUND)
  file(GLOB BM_SOURCES *_benchmark.cpp)
  foreach(BM_SOURCE ${BM_SOURCES})
    get_filename_component(OUTPUT ${BM_SOURCE} NAME_WE)
    message("building ${OUTPUT} from ${BM_SOURCE}")

    # for each file, build a binary
    add_executable(${OUTPUT} ${BM_SOURCE})

    # include openpivcore
    include_directories(${CMAKE_SOURCE_DIR})
    target_link_libraries(
      ${OUTPUT}
      PRIVATE openpivcore
      PRIVATE benchmark::benchmark
      PRIVATE benchmark::benchmark_main)
  endforeach()
endif()
