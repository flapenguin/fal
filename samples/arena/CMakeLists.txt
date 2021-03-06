cmake_minimum_required (VERSION 3.1)

project(fal-arena-gc)
set_property(GLOBAL PROPERTY C_STANDARD 99)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../include")

# Samples are always debug.
set(CMAKE_BUILD_TYPE Debug)

if(MSVC)
  add_definitions(-Dinline=__inline) # MSVC cannot into proper C99.
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Wall")
elseif(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -pedantic -Wextra")
endif()

add_executable(semispace-gc semispace-gc.c)
add_executable(mark-n-sweep-gc mark-n-sweep-gc.c)
add_executable(mark-n-compact-gc mark-n-compact-gc.c)
add_executable(microalloc microalloc.c)