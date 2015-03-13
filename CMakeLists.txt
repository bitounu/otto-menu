cmake_minimum_required(VERSION 2.8)
project(otto_menu)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(OTTO_GFX   "deps/otto-gfx")
set(OTTO_SDK   "deps/otto-sdk")
set(OTTO_UTILS "deps/otto-utils")
set(STAK_HW    "deps/libStakHardware")
set(ENTITYX    "deps/entityx")

add_subdirectory(${OTTO_GFX})
add_subdirectory(${STAK_HW})

set(ENTITYX_DT_TYPE float)
set(ENTITYX_BUILD_SHARED OFF CACHE BOOL "No shared libs plz")
set(CROSS_COMPILING ON) # Needed for EntityX's C++11 checks
add_subdirectory(${ENTITYX})

include_directories(
  deps/Choreograph/src
  ${ENTITYX}
  ${OTTO_GFX}/deps/nanosvg/src
  ${OTTO_GFX}/deps/glm/glm
  ${OTTO_GFX}/src
  ${OTTO_SDK}/src
  ${OTTO_UTILS}/src
  ${STAK_HW}/include
  lib)

include_directories(SYSTEM "/opt/vc/include") # VideoCore

file(GLOB deps_src
  "deps/Choreograph/src/choreograph/*.cpp"
  "deps/otto-utils/src/*.cpp")
file(GLOB src "src/*.cpp")

set(otto_menu_src ${deps_src} ${src})

set_source_files_properties(${otto_menu_src} PROPERTIES COMPILE_FLAGS
  "-include make_unique.hpp -include algorithm")

add_library(otto_menu SHARED ${otto_menu_src})
target_link_libraries(otto_menu otto_gfx StakHardware entityx)

# Copy assets to the build directory
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/assets
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
  DEPENDS ${CMAKE_SOURCE_DIR}/assets
  COMMENT "Copying assets")
add_custom_target(otto_menu_assets ALL DEPENDS ${CMAKE_BINARY_DIR}/assets)
add_dependencies(otto_menu otto_menu_assets)
