cmake_minimum_required(VERSION 2.8)
project(otto_menu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1y -stdlib=libc++")

set(OTTO_GFX "deps/otto-gfx")
set(OTTO_SDK "deps/otto-sdk")

add_subdirectory(${OTTO_GFX})

include_directories(
  deps/Choreograph/src
  ${OTTO_GFX}/deps/nanosvg/src
  ${OTTO_GFX}/deps/glm/glm
  ${OTTO_GFX}/src
  ${OTTO_SDK}/src)

include_directories(SYSTEM "/opt/vc/include") # VideoCore

file(GLOB deps_src "deps/Choreograph/src/choreograph/*.cpp")
file(GLOB src "src/*.cpp")

add_library(otto_menu SHARED ${deps_src} ${src})
target_link_libraries(otto_menu otto_gfx c++abi)
