#pragma once
#include <vector>
#include <span>
#include <vulkan/vulkan_core.h>

struct DescriptorLayoutBuilder{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);
    void clear();
    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator
{
    struct PoolSizeRatio{
        VkDescriptorType type;
        float ratio;
    };

    /*
    ** Its possible to have one very big descriptor pool that handles the entire engine,
    ** but that means we need to know what descriptors we will be using for everthing ahead
    ** we keep it simpler an we'll have multiple descriptor pools for different parts of the project
     */
    VkDescriptorPool pool; // for descriptor allocation, need to be preinitialized with some size and types of descriptors

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio>);
    void clear_descriptors(VkDevice device);
    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

};
