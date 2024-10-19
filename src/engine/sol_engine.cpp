#include "sol_engine.h"

#include "VkBootstrap.h"
#include "sol_initializers.h"
#include "sol_types.h"
#include "sol_images.h"

#include <GLFW/glfw3.h>
#include <fmt/core.h>

#include <cassert>
#include <chrono>
#include <cmath>
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
SolEngine::create_swapchain(uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};

    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    // use designated initializers & methodchaining (each method return a
    // reference to swapchainbuilder which allows successive method calls)
    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{.format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
        // use hard vsync present mode, will limit the fps to the speed of the monitor
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    _swapchainExtend = vkbSwapchain.extent;
    // store swapchain and its related images
	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void
SolEngine::cleanup()
{

    if (_isInitialized)
    {
        // cleaning up order is the opposite of creation process
        // make sure the gpu has stopped working
        vkDeviceWaitIdle(_device);

        for(int i = 0; i < FRAME_OVERLAP; i++)
        {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            // destroy sync objects
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
        }

        destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);

        vkDestroyInstance(_instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void
SolEngine::destroy_swapchain()
{
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    // destroy swapchain resources
    for (int i = 0; i < _swapchainImageViews.size(); i++)
    {
        vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
    }
}

void
SolEngine::draw()
{
    // wait until the gpu has finished rendering the last frame. Timeout of 1 second
    // when setting timeout to 0ns we could check if the GPU is still busy with commandexecution
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._renderFence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._renderFence)); // Fences need to be reset btw. uses, can't use same fence on multiple GPU commands without resetting

   // request image from the swapchain
   uint32_t swapchainImageIndex;
   VK_CHECK(vkAcquireNextImageKHR(_device
                                  , _swapchain, 1000000000
                                  , get_current_frame()._swapchainSemaphore
                                  , nullptr
                                  , &swapchainImageIndex));
   VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;

   // now that we are sure that the commands finished executing, we can safely
   // reset the command buffer to begin recording again.
   VK_CHECK(vkResetCommandBuffer(cmd, 0));

   VkCommandBufferBeginInfo commandBufferInfo = {};
   commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   commandBufferInfo.pNext = nullptr;
   commandBufferInfo.pInheritanceInfo = nullptr;
   commandBufferInfo.flags
       = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // we use this buffer only
                                                      // once

   // start recording:
   VK_CHECK(vkBeginCommandBuffer(cmd, &commandBufferInfo));

   SolUtil::transition_image(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

   // make a clear-color from a frame
   // flash with a 120 fram period
   VkClearColorValue clearValue;
   float flash = std::abs(std::sin(_frameNumber / 120.0f));
   clearValue = {{0.0f, 0.0f, flash, 1.0f}};

   VkImageSubresourceRange clearRange = SolInit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

  // clear image
   vkCmdClearColorImage(cmd, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL,
                        &clearValue, 1, &clearRange);

   // make the swapchain image into presentable mode
   SolUtil::transition_image(cmd, _swapchainImages[swapchainImageIndex],
                             VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

   // finalize rec & command buffer (we can no longer add commands), can now be executed
   VK_CHECK(vkEndCommandBuffer(cmd));

   // prepare the submission to the queue
   // we want to wait on the _presentSemaphore, as that semephore is signaled when the swapchain is ready
   // we will signal the _renderSemaphore, to signal that rendering is finished
   VkCommandBufferSubmitInfo cmdinfo = SolInit::command_buffer_submit_info(cmd);

   VkSemaphoreSubmitInfo waitInfo = SolInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                                                                   get_current_frame()._swapchainSemaphore);
   VkSemaphoreSubmitInfo signalInfo = SolInit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                                                                     get_current_frame()._renderSemaphore);

   VkSubmitInfo2 submit = SolInit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

   // submit command buffer to the queue and execute it
   // _renderfence will now block until the graphic commands finish execution
   VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit
                           , get_current_frame()._renderFence));

   // prepare present
   // this will put the image we just renered to into the visible window
   // we want to wait on the _renderSemaphore for that,
   // as its necessary that drawing commands have finished before the image is displayed
   VkPresentInfoKHR presentInfo = {};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
   presentInfo.pNext = nullptr;
   presentInfo.pSwapchains = &_swapchain;
   presentInfo.swapchainCount = 1;

   presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
   presentInfo.waitSemaphoreCount = 1;

   presentInfo.pImageIndices = &swapchainImageIndex;

   VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

   // increase the number of frames drawn
   _frameNumber++;
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
    // general workflow of commands:
    // 1. allocate VkCommandBuffer from a VkCommandPool
    // 2. record commands into the command buffer, using VkCmdXXXX functions
    // 3. submit the command buffer into a VkQueue which starts executing the commands

    // 1a create a command pool for commands submitted to the graphics queue

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.flags
        = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // we also want to
                                                           // reset individual
                                                           // commands,
                                                           // alternative is to
                                                           // reset the pool
                                                           // which resets ALL
                                                           // commands
    commandPoolInfo.queueFamilyIndex = _graphicsQueueFamily;

    for(int i = 0; i < FRAME_OVERLAP; i++) {
      VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

      // 1b allocate the default command buffer that we will use for rendering
      VkCommandBufferAllocateInfo cmdAllocInfo = {};
      cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdAllocInfo.pNext = nullptr;
      cmdAllocInfo.commandPool = _frames[i]._commandPool;
      cmdAllocInfo.commandBufferCount = 1; // allows to alloc multiple buffers at once
      cmdAllocInfo.level
          = VK_COMMAND_BUFFER_LEVEL_PRIMARY; // primaries are sent to the
                                             // VkQueue, secondaries are most
                                             // commonly used as subcommands
                                             // from multiple threads

      VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));
    }
}

void
SolEngine::init_swapchain()
{
    // size of the swapchainimages need to have the size of the surface window
    create_swapchain(_windowExtend.width, _windowExtend.height);
}

void
SolEngine::init_sync_structures()
{
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i]._renderFence));

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i]._renderSemaphore));
    }
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

    // due vkbootstrap get directly the graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily
        = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
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
