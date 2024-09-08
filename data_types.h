#pragma once

#include <array>
#include <glm/fwd.hpp>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES /// force automatically alignmenst of
                                           /// basic datatypes
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>                  // llinear algebra types
#include <glm/gtc/matrix_transform.hpp> /// he perspective projection matrix generated by GLM will use the OpenGL depth range of -1.0 to 1.0 by default. We need to configure it to use the Vulkan range of 0.0 to 1.0 using the GLM_FORCE_DEPTH_ZERO_TO_ONE definition.
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

const uint32_t WINDOW_WIDTH = 1280;
const uint32_t WINDOW_HEIGHT = 1024;

enum class Axis { X, Y, Z };

enum class Model {
    TestRectangle = 0,
    Earth3D,
    Earth3Dv3,
    VikingRoom,
};

static const std::map<Model, std::string> textureMap = {
    {Model::TestRectangle, "textures/texture.jpg"},
    {Model::VikingRoom, "textures/viking_room.png"},
    {Model::Earth3D, "textures/texture_earth2.jpg"},
    {Model::Earth3Dv3, "textures/texture_earth3.jpg"},

};

static const std::map<Model, std::string> modelMap
    = {{Model::VikingRoom, "models/viking_room.obj"},
       {Model::Earth3D, "models/earth2.obj"},
       {Model::Earth3Dv3, "models/earth3.obj"}

};

const int MAX_FRAMES_IN_FLIGHT
    = 2; /// how many frames should be processed concurrently ?

const std::vector<const char *> validationLayers
    = {"VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions
    = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// a limited amount of states can be changed without recreating the pipeline at
// draw time
const std::vector<VkDynamicState> dynamicStates
    = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

// clang-format off
// check if its a debug-build or not
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif
// clang-format on

struct QueueFamilyIndices {
    // graphicsFamily could have a value or not
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/*
** Basically three kinds of properties need to be checked
** 1. basic surface capibilities (min/max number of images in swap chain,
*     min/max width/height of images)
** 2, surface formats (pixel format, color space)
** 3. available presentation modes
*/
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord; /// uv coordinates. The texture coordinates determine
                        /// how the image is actually mapped to the geometry

    /** vertex binding describes at which rate to load data from memory
     * throughout the vertices, specifies nr of bytes between data entries and
     * whether to move to the next data entry after each vertex or after each
     * instance*/
    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate
            = VK_VERTEX_INPUT_RATE_VERTEX; /// no instanced-rendering
        return bindingDescription;
    }

    /**
     *  An attribute description struct describes how to extract a vertex
     * attribute from a chunk of vertex data originating from a binding
     * description.
     * */
    static std::array<VkVertexInputAttributeDescription, 3>
    getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3>
            attributeDescriptions{};
        attributeDescriptions[0].binding
            = 0; /// from which binding the data comes from
        attributeDescriptions[0].location
            = 0; /// references the location directive of the input in the
                 /// vertex shader (location 0 in vertex shader is "position",
                 /// with 2 32bit float components)
        attributeDescriptions[0].format
            = VK_FORMAT_R32G32B32_SFLOAT; /// these shader types & format are
                                          /// commonly used together: float:
                                          /// VK_FORMAT_R32_SFLOAT,
                                          /// vec2:VK_FORMAT_R32G32_SFLOAT,
                                          /// vec3: VK_FORMAT_R32G32B32_SFLOAT,
                                          /// vec4:
                                          /// VK_FORMAT_R32G32B32A32_SFLOAT

        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        // the color attribute is much the same

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    // operator overloadiing needed bc we use a userdefined type (Vertex) as a
    // key in a map (uniqueVertices)
    bool operator==(const Vertex &other) const
    {
        return pos == other.pos && color == other.color
               && texCoord == other.texCoord;
    }
};

//  needed bc we use a userdefined type (Vertex) as a
// key in a map (uniqueVertices)
namespace std {
template <> struct hash<Vertex> {
    size_t operator()(Vertex const &vertex) const
    {
        return ((hash<glm::vec3>()(vertex.pos)
                 ^ (hash<glm::vec3>()(vertex.color) << 1))
                >> 1)
               ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

// position and color values and texture cooridnates combined in one array of
// vertices (= interleaving vertex attributes)
const std::vector<Vertex> verticesRectangle = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.8f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, -0.8f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.8f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.8f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

};

// index buffer related for removing duplicated vertices
const std::vector<uint16_t> indicesRectangle
    = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4

}; /// atm we sticking to max ~65k vertices (16bit)

/**
 * UBO as a descriptor set layout which specifies the types of ressources going
 * to be accesed by the pipeline Usage of descriptors consists of three parts:
 * - Specify a descriptor set layout during pipeline creation
 * - Allocate a descriptor set from a descriptor pool
 * - Bind the descriptor set during rendering
 * A descriptor set specifies the actual buffer or image resources that will be
 * bound to the descriptors, just like a framebuffer specifies the actual image
 * views to bind to render pass attachments. The descriptor set is then bound
 * for the drawing commands just like the vertex buffers and framebuffer.
 *
 * Alignement of UBO-members: Vulkan expects the data in your structure to be
 * aligned in memory in a specific way
 * - Scalars have to be aligned by N (= 4 bytes given 32 bit floats).
 * - A vec2 must be aligned by 2N (= 8 bytes)
 * - A vec3 or vec4 must be aligned by 4N (= 16 bytes)
 * - A nested structure must be aligned by the base alignment of its members
 * rounded up to a multiple of 16.
 * - A mat4 matrix must have the same alignment as a vec4.
 * */
struct UniformBufferObject {
    alignas(16) glm::mat4
        model; // its a good reason to always be explicit about the alignment
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
