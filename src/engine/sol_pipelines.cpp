#include "sol_initializers.h"
#include "sol_pipelines.h"

#include <vulkan/vulkan_core.h>

#include <fstream>


bool
solutil::load_shader_module(const char *filePath, VkDevice device, VkShaderModule* outShaderModule)
{
    // open the file. cursor at the end
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (not file.is_open())
    {
        fmt::print("ShaderFile NOT opened from {}", filePath);
        return false;
    }

    fmt::print("ShaderFile opened from {} \n", filePath);
    // find what the size of the file is by looking up the location of the cursor
    // bc cursor is at the end it gives the size directly in bytes
    size_t fileSize = (size_t)file.tellg();
    fmt::print("ShaderFile-size: {}\n", fileSize);

    // spirv expects the buffer to be uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);

    // load the entire file into the buffer
    file.read((char*)buffer.data(), fileSize);

    // when the file is loaded into the buffer, we can clode the file
    file.close();

    // create a new ShaderModule, using the buffer we loaded
    VkShaderModuleCreateInfo createInfo = {};
     createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
     createInfo.pNext = nullptr;

    // codeSize has to be in bytes, so we multiply the ints in the buffer by the size of int
    // to get the real size of the buffer
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data(); // shadercoder-data as an int-array

    // check that the creation goes well
    VkShaderModule shaderModule;
    if(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        fmt::print("ShaderMoudle creation failed");
        return false;
    }

    *outShaderModule = shaderModule; // shadermodules are only needed when building a pipeline and
                                     // once the pipeline is built they can be safely destroyed, so we wont
                                     // be storing them in the VulkanEngine class.
    return true;
}
