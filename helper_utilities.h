#pragma once

#include <fstream>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_core.h>

VkResult
CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger)
{
    // extension function is not automatically loaded, tf look up its adress
    // will return nullptr if the function couldn't be loaded
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void
DestroyDebugUtilsMessengerEXT(VkInstance instance,
                              VkDebugUtilsMessengerEXT debugMessenger,
                              const VkAllocationCallbacks *pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

/**
 * Helper function to open the shader files
 */
static std::vector<char>
readFile(const std::string &filename)
{
    // start at the end of the file (ate) / binary
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (not file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    // bc of reading at the end we can determine the size of the file and
    // allocate a buffer
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    std::cout << "read bufferSize: " << fileSize << std::endl;

    // the seek back and read data at once
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}
