#pragma once

#include <fmt/core.h>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::print("Detected Vulkan error: {}\n", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
