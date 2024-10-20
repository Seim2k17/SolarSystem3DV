#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace SolUtil {

    void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
    void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
}
