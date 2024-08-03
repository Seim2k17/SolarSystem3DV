
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
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER "gcc")

find_package(fmt REQUIRED)

add_executable(vulkanTut "main.cpp")#${MAIN})

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
  pthread
  X11
  Xxf86vm
  Xrandr
  Xi
  fmt::fmt
  stdc++
)

add_custom_target(cleanup COMMAND rm -rf *)
