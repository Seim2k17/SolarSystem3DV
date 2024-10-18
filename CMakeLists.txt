
cmake_minimum_required(VERSION 3.7...3.26)


if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(SolarEngine VERSION 1.0
  DESCRIPTION "Earth System 3D with real NASA Data"
  LANGUAGES CXX
)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "g++")
set(INCLUDE_THIRDPARTY_FOLDER "includeLibs/")
set(INCLUDE_IMGUI_FOLDER "imgui/")
set(IMGUI_DIR imgui)
set(IMGUI_FILES
  ${IMGUI_DIR}/imgui_impl_vulkan.cpp
  ${IMGUI_DIR}/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_stdlib.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
)
find_package(fmt REQUIRED)
include_directories(${IMGUI_DIR})

set(MAIN "src/main.cpp")

set (SRC
  "src/engine/sol_engine.cpp"
  "src/engine/sol_images.cpp"

)

set (HEADER
  "src/engine/sol_engine.h"
  "src/engine/sol_types.h"
  "src/engine/sol_images.h"
)

set(ENGINE
  ${SRC}
  ${HEADER}
)

add_executable(earth3D
  ${MAIN}
  ${ENGINE}
  ${IMGUI_FILES}
)

target_include_directories(earth3D PUBLIC "${INCLUDE_THIRDPARTY_FOLDER}")

# Set compile options
target_compile_options(earth3D PRIVATE
  -std=c++17
  -O2
)

add_subdirectory(third_party)

# Set link options
target_link_libraries(earth3D PRIVATE
  glfw
  vulkan
  vkbootstrap
  dl
  #mvec
  #m
  pthread
  X11
  Xxf86vm
  Xrandr
  #Xi
  fmt::fmt
  stdc++
  -lm
)

add_custom_target(cleanup COMMAND rm -rf *)
