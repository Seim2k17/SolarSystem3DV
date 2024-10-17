find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)

target_sources(vkbootstrap PRIVATE
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
)

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)
