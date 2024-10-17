#include "sol_engine.h"

#include "VkBootstrap.h"
#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>
#include <vulkan/vulkan_core.h>

constexpr bool bUseValidationLayers = true;

// TODO: howto create uniqueptr from it
// std::unique_ptr<SolEngine> loadedEngine = nullptr;
SolEngine *loadedEngine = nullptr;

// static function bc GLFW does not know how to properly call a member
// function with the right this pointer to our instance
static void
framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto app = reinterpret_cast<SolEngine *>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

/*
** Key callback function from GLFW, when pressing acertain key this callback is
*executed
**
*/
static void
key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        fmt::print("ESC pressed\n");
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void
SolEngine::cleanup()
{

    if (_isInitialized)
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void
SolEngine::draw()
{
    // t.b.a
}

SolEngine &
SolEngine::Get()
{
    return *loadedEngine;
}

void
SolEngine::init()
{
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    initWindow();

    init_vulkan();
    init_swapchain();
    init_commands();
    init_sync_structures();

    // everything went fine
    _isInitialized = true;
}
void
SolEngine::init_commands()
{
    // t.b.a
}

void
SolEngine::init_swapchain()
{
    // t.b.a
}

void
SolEngine::init_sync_structures()
{
    // t.b.a
}

void
SolEngine::init_vulkan()
{
    vkb::InstanceBuilder builder;

    auto solInstance = builder.set_app_name("SolarSystem 3D")
        .request_validation_layers(bUseValidationLayers)
        .use_default_debug_messenger()
        .require_api_version(1, 3, 0) // need at least vulkan 1.3
        .build();

    vkb::Instance vkb_inst = solInstance.value();

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    setupWindowSurface();

    VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    vkb::PhysicalDevice physicalDevice = selector
        .set_minimum_version(1, 3)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .set_surface(_surface)
        .select()
        .value();

    vkb::DeviceBuilder deviceBuilder{physicalDevice};

    vkb::Device vkbDevice = deviceBuilder.build().value();

    _device = vkbDevice.device;
    _chosenGPU = physicalDevice.physical_device;

}

void
SolEngine::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API,
                   GLFW_NO_API); /// do not a OpenGL context
    glfwWindowHint(GLFW_RESIZABLE,
                   GLFW_FALSE); // FIXME: resizing window lead to crashes,
                                // so I disabled it for now using a bigger
                                // resolution instead
    window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "Earth 3D", nullptr,
        nullptr); /// optionally specify a monitor to open the window on,
                  /// last parameter relevant to OpenGL
    glfwSetWindowUserPointer(window, this); /// store an arbitrary
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    glfwSetKeyCallback(window, key_callback);
}

void
SolEngine::run()
{

    // main loop
    while (not glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // TODO check for minimized window not to render
        // if(minimized) stop_rendering = true;

        // do not draw if we are minimized
        if (stop_rendering)
        {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw(); // bla
    }
}

void
SolEngine::setupWindowSurface()
{
    //TODO: check windows code

#ifdef WIN32
        // Windows specific code:
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(window);
        createInfo.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &_surface)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
#endif

#ifdef __linux__
        if (glfwCreateWindowSurface(_instance, window, nullptr, &_surface)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window!");
        }
#endif

}
