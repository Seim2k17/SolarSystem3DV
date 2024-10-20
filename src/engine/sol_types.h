#pragma once

#include <vk_mem_alloc.h>

#include <fmt/core.h>
#include <vulkan/vk_enum_string_helper.h>

#include <queue>
#include <functional>

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)

/*
** With adding more and more vulkan objects to the engine we need to handle
** their destruction
** a common approach is a deletion queue which deletes all the objects in the
** correct order
*/
struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) { // store a callback, to be more efficient an array of object handles
        deletors.push_back(function);
    }

    void flush() {
        // reverse iterate the deletion queue to execute all the functions
        for(auto it= deletors.rbegin();it != deletors.rend(); it++)
        {
            (*it)(); // call functors
        }

        deletors.clear();
    }

};


/*
** Although drawing the image directly into the swapchain is fine, but come with some restrictions, like low precission.
** So we render into a separate image. Then we copy that image to the swapchain image and present it to the screen.
**
*/
struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};
