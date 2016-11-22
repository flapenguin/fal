cmake_minimum_required (VERSION 3.1)

project(fal)
enable_testing()

set_property(GLOBAL PROPERTY C_STANDARD 99)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
  # MSVC cannot into proper C99.
  add_definitions(-Dinline=__inline)

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall")
elseif(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra")
endif()

# Tests are always debug.
set(CMAKE_BUILD_TYPE Debug)

# http://stackoverflow.com/questions/7787823/cmake-how-to-get-the-name-of-all-subdirectories-of-a-directory
macro(subdirlist result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
      list(APPEND dirlist ${child})
    endif()
  endforeach()
  set(${result} ${dirlist})
endmacro()

subdirlist(suites "${CMAKE_CURRENT_SOURCE_DIR}/test")
foreach(suite ${suites})
  file(GLOB cases "${CMAKE_CURRENT_SOURCE_DIR}/test/${suite}/*.c")

  foreach(case ${cases})
    get_filename_component(case_name ${case} NAME)
    string(REGEX REPLACE ".c$" "" case_name ${case_name})
    set(case_bin_name "test-${suite}-${case_name}")

    add_executable(${case_bin_name} ${case})
    set_target_properties(${case_bin_name}
      PROPERTIES RUNTIME_OUTPUT_DIRECTORY "test/")

    add_test(NAME "${suite}/${case_name}"
      COMMAND ${case_bin_name})
  endforeach()
endforeach()
