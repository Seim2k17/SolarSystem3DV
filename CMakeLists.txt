
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
  "src/engine/sol_descriptors.cpp"
  "src/engine/sol_images.cpp"
  "src/engine/sol_initializers.cpp"
  "src/engine/sol_pipelines.cpp"

)

set (HEADER
  "src/engine/sol_engine.h"
  "src/engine/sol_descriptors.h"
  "src/engine/sol_images.h"
  "src/engine/sol_initializers.h"
  "src/engine/sol_pipelines.h"
  "src/engine/sol_types.h"
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
  vma
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

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    )
