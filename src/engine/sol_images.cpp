#include "sol_images.h"
#include "sol_initializers.h"
#include <vulkan/vulkan_core.h>

void SolUtil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    // A pipeline barrier can be used for many different things like
    // syncronizing read/write operation between commands and controlling things
    // like one command drawing into a image and other command using that image
    // for reading.
    VkImageMemoryBarrier2 imageBarrier {
       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
       .pNext = nullptr,
       .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, // will stop gpu-commands completely when it arrives at the barrier
       .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
       .dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
       .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
       .oldLayout = currentLayout,
       .newLayout = newLayout
    };

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = SolInit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}
