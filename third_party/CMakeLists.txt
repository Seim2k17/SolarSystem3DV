find_package(Vulkan REQUIRED)

add_library(vkbootstrap STATIC)
add_library(vma INTERFACE)

target_sources(vkbootstrap PRIVATE
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
)


target_sources(vma PRIVATE
  vma/vk_mem_alloc.h
)
target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

# vma, glm are header-only so we only need the include path
target_include_directories(vma INTERFACE vma)
