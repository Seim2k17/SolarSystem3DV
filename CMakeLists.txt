
cmake_minimum_required(VERSION 3.7...3.26)


if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(VulkanTutorial1 VERSION 1.0
  DESCRIPTION "Learning Vulkan with official Khronos Vukan Tutorial"
  LANGUAGES CXX
)

set(MAIN "main.cpp")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "g++")
set(INCLUDE_THIRDPARTY_FOLDER "includeLibs/")
set(INCLUDE_IMGUI_FOLDER "imgui/")
find_package(fmt REQUIRED)
set(IMGUI_DIR imgui)
include_directories(${IMGUI_DIR})

add_executable(vulkanTut "main.cpp" ${IMGUI_DIR}/imgui_impl_vulkan.cpp ${IMGUI_DIR}/imgui_impl_glfw.cpp ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_demo.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_stdlib.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp)
target_include_directories(vulkanTut PUBLIC "${INCLUDE_THIRDPARTY_FOLDER}")

# Set compile options
target_compile_options(vulkanTut PRIVATE
  -std=c++17
  -O2
)

# Set link options
target_link_libraries(vulkanTut PRIVATE
  glfw
  vulkan
  dl
  #mvec
  #m
  pthread
  X11
  Xxf86vm
  Xrandr
  Xi
  fmt::fmt
  stdc++
  -lm
)

add_custom_target(cleanup COMMAND rm -rf *)
