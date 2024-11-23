#pragma once

#include "sol_types.h"

namespace solutil{
    bool load_shader_module(const char* filePath, VkDevice device, VkShaderModule* outSHaderModule);
}
