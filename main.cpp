#include "data_types.h"
#include "helper_utilities.h"
#define STB_IMAGE_IMPLEMENTATION
#include "includeLibs/stb_image.h"

#include <algorithm>
#include <array>
#include <bits/stdint-uintn.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glm/ext/matrix_transform.hpp>
#include <ios>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3native.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define VK_USE_PLATFORM_WIN32_KHR /// Platform windows support ...
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/fwd.hpp>
#include <glm/glm.hpp> // llinear algebra types
#include <glm/gtc/matrix_transform.hpp>

class TriangleApp {
  public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

  private:
    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // check if all defined validationlayers exists in the
        // availableLayersVector
        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (not layerFound)
            {
                return false;
            }
        }

        return true;
    }

    /* Vulkan does not have the concept of a default framebuffer, it requires an
     * infrastructure that will own the buffers we will render to, before
     * visualizing them on the screen. This is known as the swap chain and must
     * be explicitly created in vulkan. Not all graphic cards are capable of
     * presenting images directly to a screen. Image prsentation is heavily tied
     * into the window system & the surfaces associated with windows , its not
     * part of Vulkan. We have to enable VK_KHR_swapchain device extension
     *
     *
     * */
    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        // check if all required extensions are available
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(
            device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                                 deviceExtensions.end());

        for (const auto &extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API,
                       GLFW_NO_API); /// do not a OpenGL context
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        window = glfwCreateWindow(
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            "Vulkan Tutorial",
            nullptr,
            nullptr); /// optionally specify a monitor to open the window on,
                      /// last parameter relevant to OpenGL
        glfwSetWindowUserPointer(window, this); /// store an arbitrary
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    // static function bc GLFW does not know how to properly call a member
    // function with the right this pointer to our instance
    static void
    framebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto app
            = reinterpret_cast<TriangleApp *>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        setupWindowSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFrameBuffers();
        createCommandPool();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        while (not glfwWindowShouldClose(window))
        {
            glfwPollEvents();
            drawFrame();
        }

        // as all operations are async in drawFrame() & when exiting the
        // mainLoop, drawing amy still be going on, cleaning things up while
        // drawing is a bad idea
        vkDeviceWaitIdle(device);
    }

    /**
     *  Rendering a frame in Vulkan consists of a common set of steps
     *  - Wait for the previous frame to finish
     *  - Acquire an image from the swap chain
     *  - Record a command buffer which draws the scene onto that image
     *  - Submit the recorded command buffer
     *  - Present the swap chain image
     * */
    void drawFrame()
    {
        // synchronization of execution on the GPU is explicit
        // the order of operations is up to us
        // many Vulkan API calls are asychronous
        // these calls will return before the operations are actually finished
        // & the order of execution is also undefined
        // each of the operations depends on the previous one finishing
        // using a semaphore to add order between queue operations
        // semaphore are used to order work inside the same & different queues
        // there two kinds of semaphores in Vulkan: binary & timeline, we use
        // binary here its either unsignaled or signaled we use it as signal
        // semaphore in one queue operation & as a wait operation in another
        // queue operation, for ordering execution on the CPU we use a fence as
        // a similar mechanism
        // -> if the host needs to know when the GPU has finished something we
        // use a fence
        // -> semaphores are used to specify the execution of order of
        // operations on the GPU
        // -> fences are used to keep the CPU&GPU in sync with each other. We
        // want to use semaphores for swapchain operations (they happen on the
        // GPU) for waiting on the previous frame to finish we want to use
        // fences, we need the host to wait (CPU)
        vkWaitForFences(device,
                        1,
                        &inFlightFences[currentFrame],
                        VK_TRUE,
                        UINT64_MAX); /// disabled with UINT64_MAX the timeout

        uint32_t imageIndex;
        VkResult result
            = vkAcquireNextImageKHR(device,
                                    swapChain,
                                    UINT64_MAX,
                                    imageAvailableSemaphores[currentFrame],
                                    VK_NULL_HANDLE,
                                    &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        { /// The swap chain has become incompatible
          /// with the surface and can no longer be
          /// used for rendering. Usually happens
          /// after a window resize.

            recreateSwapChain();
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        { /// VK_SUBOPTIMAL_KHR: The swap chain
          /// can still be used to successfully
          /// present to the surface, but the
          /// surface properties are no longer
          /// matched exactly.
            throw std::runtime_error("failed to aquire swap chain image!");
        }

        updateUniformBuffer(currentFrame);

        // only reset the fence if we are submitting work
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        // with the imageIndex spec. the swapchain image we can now record the
        // command buffer
        //
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);
        // queue submission of the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[]
            = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        // for details see Tutorial: submitting the command buffer
        // which semaphore to wait on before the execution begins & in which
        // stages the pipeline to wait
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        // which command buffer to submit for execution
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        // which semaphores to signal once the command buffer(s) have finished
        // execution
        VkSemaphore signalSemaphores[]
            = {renderFinishedSemaphores[currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(
                graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame])
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        // last step to draw is submitting the result back to the swap chain to
        // show it on the screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr; // optional

        // OMG: after >1400 lines of code we see a triangle. Congratulation :D
        result = vkQueuePresentKHR(presentQueue, &presentInfo);

        // returns the same values as vkAquireNextImageKHR, also recreate
        // swapChain if its suboptimal, bc we want the best possible result
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
            || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }
        // advance to the next frame
        currentFrame
            = (currentFrame + 1)
              % MAX_FRAMES_IN_FLIGHT; /// By using the modulo (%) operator, we
                                      /// ensure that the frame index loops
                                      /// around after every
                                      /// MAX_FRAMES_IN_FLIGHT enqueued frames.
    }

    /**
     *  Generate a new transformation every frame to make the geometry spin
     * around. Using a UBO this way is not the most efficient way to pass
     * frequently changing values to the shader. A more efficient way to pass a
     * small buffer of data to shaders are push constants. Upcoming !
     * */
    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(
                         currentTime - startTime)
                         .count();

        UniformBufferObject ubo{};
        /**
         * The glm::rotate function takes an existing transformation, rotation
         * angle and rotation axis as parameters. The glm::mat4(1.0f)
         * constructor returns an identity matrix. Using a rotation angle of
         * time * glm::radians(90.0f) accomplishes the purpose of rotation 90
         * degrees per second.
         * */
        ubo.model = glm::rotate(glm::mat4(1.0f),
                                time * glm::radians(90.0f),
                                glm::vec3(.0f, .0f, 1.0f));
        /**
         * view transformation: look at the geometry from above at a 45 degree
         * angle. The glm::lookAt function takes the eye position, center
         * position and up axis as parameters.
         * */
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f),
                               glm::vec3(0.0f, 0.0f, 0.0f),
                               glm::vec3(0.0f, 0.0f, 1.0f));

        /**
         * Perspective projection with a 45 degree vertical field-of-view. The
         * other parameters are the aspect ratio, near and far view planes. It
         * is important to use the current swap chain extent to calculate the
         * aspect ratio to take into account the new width and height of the
         * window after a resize.
         * */
        ubo.proj = glm::perspective(glm::radians(45.0f),
                                    swapChainExtent.width
                                        / (float)swapChainExtent.height,
                                    0.1f,
                                    10.0f);
        // GLM was originally designed for OpenGL, where the Y coordinate of the
        // clip coordinates is inverted. The easiest way to compensate for that
        // is to flip the sign on the scaling factor of the Y axis in the
        // projection matrix. If you don’t do this, then the image will be
        // rendered upside down
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void cleanup()
    {
        cleanUpSwapChain();

        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyImageView(device, textureImageView, nullptr);
        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyDescriptorPool(
            device, descriptorPool, nullptr); // also cleans up DescriptorSets
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);

        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vkFreeMemory(device, vertexBufferMemory, nullptr);

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

        vkDestroyRenderPass(device, renderPass, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

        // destroy the instance right before the window
        vkDestroyDevice(device, nullptr);
        // must be destroyed before the instance -> to validate all code after
        // this we can use a separate debug utils messenger
        if (enableValidationLayers)
        {
            DestroyDebugUtilsMessengerEXT(instance, debugMesseger, nullptr);
        }

        vkDestroySurfaceKHR(instance,
                            surface,
                            nullptr); /// surface need to be destroyed before
                                      /// the instance destruction !
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        std::cout << "Cleanup!" << std::endl;
        glfwTerminate();
    }

    void cleanUpSwapChain()
    {
        for (auto framebuffer : swapChainFramebuffers)
        {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto imageView : swapChainImageViews)
        {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    /**
     * There is no global state in Vulkan and all per-application state is
     * stored in a VkInstance object. Creating a VkInstance object initializes
     * the Vulkan library and allows the application to pass information about
     * itself to the implementation.
     * */
    void createInstance()
    {
        // 0. check for validationLayers (debugging)
        if (enableValidationLayers && not checkValidationLayerSupport())
        {
            throw std::runtime_error(
                "validation layers requested, but not available!");
        }

        // 1. set information about application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // 2. another nonoptional struct to fill for the instance
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // GLFW has built-in function that returns the extensionscount, for
        // standard
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtension;
        glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(
            extensions.size()); // standard: glfwExtensionCount;
        createInfo.ppEnabledExtensionNames
            = extensions.data(); // stadnard: glfwExtension;
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        // global validation layer determination, only available in debug build
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount
                = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            debugCreateInfo.pNext
                = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        } else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // could check for extension support (see instance page bottom)
        // ...
        //
        // 3. finally create the instance and check result
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance");
        }
    }

    // for setting up a callback to handle messages and details for the
    // validation layer
    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(
            glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (enableValidationLayers)
        {
            extensions.push_back(
                VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // to avoid typos we use the
                                                    // macro
        }

        return extensions;
    }

    // debug callback, VKAPI_ATTR / VKAPI_CALL ensure the right signature for
    // Vulkan
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT
                      messageSeverity, /// Diag, Info, prop. Bug, Invalid
                  VkDebugUtilsMessageTypeFlagsEXT
                      messageType, /// unrelated to spec or perf.,
                                   /// spec-violation, non-optimal Vulkan-use
                  const VkDebugUtilsMessengerCallbackDataEXT
                      *pCallbackData, /// details of the message
                  void *pUserData)    /// pass own userdata
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage
                  << std::endl;

        return VK_FALSE; /// if true the call is aborted with
                         /// VK_ERROR_VALIDATION_FAILED_EXT
    }

    // window need to be setup right after the instance creation, it can
    // influence the physical device selection window surfaces are optional
    // component of vulkan (e.g. one need off-screen rendering)
    void setupWindowSurface()
    {
#ifdef WIN32
        // Windows specific code:
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(window);
        createInfo.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
#endif

#ifdef __linux__
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window!");
        }
#endif
    }

    // before drawing, even before almost every operation in Vulkan commands
    // need to be submitted to a queue, there are differet types of queues that
    // are originated in different queue families, each family only allows
    // processing of certain commands we need to check which queue families are
    // supported by the device for now we only looking for a queu that supports
    // graphic commands
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        // logic to find graphics queue family
        QueueFamilyIndices indices;
        // 1. get the queueFamilyCount
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(
            device, &queueFamilyCount, nullptr);

        // 2. then get the properties in the exactly length
        // VkQueueFamilyProperties - struct contains details about the queue
        // family, incl. type of operations that are supported

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(
            device, &queueFamilyCount, queueFamilies.data());

        // for now we need a queue that supports >VK_QUEUE_GRAPHICS_BIT

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(
                device, i, surface, &presentSupport);

            // bc it does not mean that every every device support the window
            // system integration so we need to find a queue-family that support
            // presenting to the surface, it's very likely that these end up
            // being the same queue family after all
            if (presentSupport)
            {
                indices.presentFamily = i;
            }
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i; // 1;
            }
            // early return we found the needed capability
            if (indices.isComplete())
            {
                break;
            }
            ++i;
        }
        return indices;
    }

    int rateDeviceSuitability(VkPhysicalDevice device)
    {
        int score = 0;

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        // Discrete GPUs have a significant performance advantage
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 1000;
        }
        // Maximum possible size of textures affects graphics quality
        score += deviceProperties.limits.maxImageDimension2D;

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        // Application can't function without geometry shaders
        if (!deviceFeatures.geometryShader)
        {
            return 0;
        }

        return score;
    }

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        // 1. basic surface capibilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            device, surface, &details.capabilities);

        // 2, surface formats (pixel format, color space)
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            // make sure that the vector can hold all formats
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                device, surface, &formatCount, details.formats.data());
        }

        // 3. available presentation modes
        uint32_t modeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface, &modeCount, nullptr);

        if (modeCount != 0)
        {
            // make sure that the evctor can hold all formats
            details.presentModes.resize(modeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device, surface, &modeCount, details.presentModes.data());
        }

        return details;
    }

    // determine the surfaceformat (color depth, color channel, colortype,
    // colorspace) BGRA
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availabeFormats)
    {
        // check if the preferred combination is available
        for (const auto &availableFormat : availabeFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB
                && availableFormat.colorSpace
                       == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }

        // If that also fails then we could start ranking the available formats
        // based on how "good" they are, but in most cases it’s okay to just
        // settle with the first format that is specified.
        return availabeFormats[0];
    }

    /*
    ** Presentation mode is the most important setting in the swap chain, it
    *  represents the actual conditions for showing images to the screen
    ** There are four possible modes available in Vulkan
    ** 1. VK_PRESENT_MODE_IMMEDIATE_KHR: images are submitted to the screen
    *right away (may result in tearing)
    ** 2. VK_PRESENT_MODE_FIFO_KHR: swap chain is a queue, display takes an
    *image from the front of the queue when the display is refreshed & the
    *program inserts rendered images at the back of the queue
    **    if the queue is full then the prgram has to wait. Its similar to
    *VSYNC. the moment that the display is refreshed is known as "vertical
    *blank"
    ** 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from 2. if
    *the app is late and the queue was empty at the last vertical blank.
    **    Instead of waiting for the next vertical blank, the image is
    *transferred right away when it finally arrives. This may result in visible
    *tearing.
    ** 4. VK_PRESENT_MODE_MAILBOX_KHR: variation of 2. Instead of blocking the
    *app when the queue is full, the images that are already queued are simply
    *     replaced with newer ones.
    **    This mode can be used to render frames as fast as possible while still
    *     avoiding tearing, resulting in fewer latency issues than standard
    *     vertical sync. Commonly known as "triple buffering", although three
    *     buffers alone does not necessarily mean that the framerate is
    *unlocked.
    */
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR> &availablePresentModes)
    {
        // VK_PRESENT_MODE_MAILBOX_KHR is a nice tradeoff if energy usage is not
        // a concern on mobile where energy usage is more important we want to
        // use VK_PRESENT_MODE_FIFO_KHR
        for (const auto &availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }

        // this value is guaranteed to be available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /*
    ** swap extend is the resolution of the swap chain images, almost always
    *equal to the resolution of the window that we're drawing in pixels
    */
    VkExtent2D chooseSwapExtend(const VkSurfaceCapabilitiesKHR &capabilities)
    {
        if (capabilities.currentExtent.width
            != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        } else
        {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtend
                = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

            actualExtend.width = std::clamp(actualExtend.width,
                                            capabilities.minImageExtent.width,
                                            capabilities.minImageExtent.width);
            actualExtend.height
                = std::clamp(actualExtend.height,
                             capabilities.minImageExtent.height,
                             capabilities.minImageExtent.height);

            return actualExtend;
        }
    }

    /**
     *Vulkan separates the concept of physical and logical devices. A physical
     *device usually represents a single complete implementation of Vulkan
     *(excluding instance-level functionality) available to the host, of which
     *there are a finite number.
     * */
    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error(
                "failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        // rate the physical device and pick the one with the highest score
        // (which def. meet the requirement)
        std::multimap<int, VkPhysicalDevice> candidates;

        for (const auto &device : devices)
        {
            int score = rateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }
        if (candidates.begin()->first > 0)
        {
            physicalDevice = candidates.rbegin()->second;
        } else
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    // check GPU for vulkan-features we need for our app, later on we add more !
    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        /* this could be a device selection process
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        std::cout << "DeviceName: " << deviceProperties.deviceName << std::endl;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        // e.g. app only usable for dedicated graphics cards that support
        geometry
        // shader
        return deviceProperties.deviceType ==
                   VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
               deviceFeatures.geometryShader;
        */
        // ensure that the device can process the commands we want to use
        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport
                = querySwapChainSupport(device);
            swapChainAdequate = not swapChainSupport.formats.empty()
                                && not swapChainSupport.presentModes.empty();
        }

        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return indices.isComplete() && extensionsSupported && swapChainAdequate
               && supportedFeatures.samplerAnisotropy;
    }

    void createLogicalDevice()
    {
        // 1. specifying details in structs
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies
            = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        // influence scheduling of command buffer execution from 0.0 .. 1.0
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy
            = VK_TRUE; /// optional feature, we need explicitly request it
        vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount
            = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        // using the swapchain : enabling the VK_KHR_swapchain
        createInfo.enabledExtensionCount
            = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        /* no device specific extension needed for now */
        /*
        createInfo.enabledExtensionCount;
        createInfo.ppEnabledExtensionNames;
        createInfo.flags;
        */
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount
                = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        } else
        {
            std::cout << "Logical device: " << device << " created."
                      << std::endl;
        }
        // as we only create a single queue from this family, we use 0 as a
        // index retrieve the  queue handle
        vkGetDeviceQueue(
            device, indices.presentFamily.value(), 0, &presentQueue);
        vkGetDeviceQueue(
            device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    }

    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport
            = querySwapChainSupport(physicalDevice);

        VkSurfaceFormatKHR surfaceFormat
            = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode
            = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtend(swapChainSupport.capabilities);

        // how many images we want to have in the swap chain, but simply
        // sticking to this minimum means that we may sometimes have to wait on
        // the driver to complete internal operations before we can acquire
        // another image to render to. Therefore it is recommended to request at
        // least one more image than the minimum
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        // we should also make sure to not exceed the maximum number of images
        // while doing this, where 0 is a special value that means that there is
        // no maximum:
        if (swapChainSupport.capabilities.maxImageCount > 0
            && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        // as familiar we need to fill a big structure
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // amount of layers each image consists of, always 1 unless we develop a
        // stereoscopic 3D app
        createInfo.imageArrayLayers = 1;
        // this specifies what kind of operations we'll use the images in the
        // swap chain for (here used as color attachment bc we render directly
        // to them) It is also possible that you’ll render images to a separate
        // image first to perform operations like post-processing. In that case
        // you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and
        // use a memory operation to transfer the rendered image to a swap chain
        // image.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // specify how to handle swap chain images that will be used across
        // multiple queue families. That will be the case in our application if
        // the graphics queue family is different from the presentation queue.
        // We’ll be drawing on the images in the swap chain from the graphics
        // queue and then submitting them on the presentation queue. There are
        // two ways to handle images that are accessed from multiple queues:
        // 1. VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family
        // at a time and ownership must be explicitly transferred before using
        // it in another queue family. This option offers the best performance.
        // 2. VK_SHARING_MODE_CONCURRENT: Images can be used across multiple
        // queue families without explicit ownership transfers.

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[]
            = {indices.graphicsFamily.value(), indices.presentFamily.value()};

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // one can apply a certainTransform (rotate, flip ...)
        createInfo.preTransform
            = swapChainSupport.capabilities.currentTransform;
        // for blending with other windows, almost always ignore the alpha
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // we dont care about the color of pixels that are obscured, better
        // performance if clipping is enabled
        createInfo.clipped = VK_TRUE;
        // eg if windows are resized, we need to build SwapChain from scratch
        // more on this later for now: its null
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain))
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(
            device, swapChain, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
    }

    VkImageView createImageView(VkImage image, VkFormat format)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;
        // allow to swizzle the  color  channels around , e.g. map all
        // channels to the red channel for a monochrome texture: here we
        // stick to the default The swizzle can rearrange the components of
        // the texel, or substitute zero or one for any components. It is
        // defined as follows for each color component:
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        // describes the purpose of the image and which part of the image
        // should be accessed here our images are color targets without
        // mipmapping levels or multiple layers
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount
            = 1; /// working with a stereographic 3D app, a swap chain with
                 /// multiple layers is needed (VR?)

        VkImageView imageView;
        if (vkCreateImageView(device, &createInfo, nullptr, &imageView)
            != VK_SUCCESS)
        { /// manual creation process demands a manual destroy !
            throw std::runtime_error("failed to create image views!");
        }
        return imageView;
    }
    /*
    ** An imagevieview is sufficient to start using an image as a texture, but
    *to be rendered a frambuffer is needed (see graphic-pipelines-creation)
    *
    *  An image view is quite literally a view into an image. It describes how
    *to access the image and which part of the image to access, for example if
    *it should be treated as a 2D texture depth texture without any mipmapping
    *  levels.
    *   */
    void createImageViews()
    {
        swapChainImageViews.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            swapChainImageViews[i]
                = createImageView(swapChainImages[i], swapChainImageFormat);
        }
    }

    /* Draw commands must be recorded within a render pass instance. Each render
     * pass instance defines a set of image resources, referred to as
     * attachments, used during rendering. Before creating the pipeline we need
     * to tell Vulkan about the framebuffer attachements that will be used while
     * rendering.
     */
    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format
            = swapChainImageFormat; /// should match the format
                                    /// of the swap chain images
        colorAttachment.samples
            = VK_SAMPLE_COUNT_1_BIT; /// no multisampling, 1 sample
        colorAttachment.loadOp
            = VK_ATTACHMENT_LOAD_OP_CLEAR; /// clear framebuffer to black before
                                           /// drawing a new frame
        colorAttachment.storeOp
            = VK_ATTACHMENT_STORE_OP_STORE; /// rendered contents will be stored
                                            /// in memory and can be read later
        colorAttachment.stencilLoadOp
            = VK_ATTACHMENT_LOAD_OP_DONT_CARE; /// we wont do anything with
                                               /// stencil buffer
        colorAttachment.stencilStoreOp
            = VK_ATTACHMENT_STORE_OP_DONT_CARE; /// so results on load and store
                                                /// is irrelevant
        colorAttachment.initialLayout
            = VK_IMAGE_LAYOUT_UNDEFINED; /// we dont care what previous layout
                                         /// the image was in
        colorAttachment.finalLayout
            = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; /// we want the img to be ready
                                               /// for presentation uisng the
                                               /// swap chain after rendering

        // every subpass references one or more attachments
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment
            = 0; /// we ony have 1 VkAttachmentDescription, so index is 0
        colorAttachmentRef.layout
            = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; /// attachment is
                                                        /// intended as a color
                                                        /// buffer with "best
                                                        /// performance"

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.colorAttachmentCount = 1;

        // subpas dep. we need to ensure that the renderpasses don't begin until
        // the image is available
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // sepcify the operations to wait on and stages in which these
        // operations occur
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        // the operations that should wait on this
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // create Renderpass itself
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createDescriptorSetLayout()
    {
        // Every binding needs to be described through a
        // VkDescriptorSetLayoutBinding struct.
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0; // binding used in the shader
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        // in which shader stage the descriptor is going to be referenced, can
        // be a combination of VkShaderStageFlagBits values or the value
        // VK_SHADER_STAGE_ALL_GRAPHICS
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // only relevant for image sampling related descriptors
        uboLayoutBinding.pImmutableSamplers = nullptr;

        // makes it possible for shaders to access an image resource through a
        // sampler object
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType
            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags
            = VK_SHADER_STAGE_FRAGMENT_BIT; // indicate that we intend to use
                                            // the combined image sampler
                                            // descripter in the fragment shader

        std::array<VkDescriptorSetLayoutBinding, 2> bindings
            = {uboLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(
                device, &layoutInfo, nullptr, &descriptorSetLayout)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // clang-format off
  /*
  ** The graphics pipeline is the sequence of operations that take the vertices
  *  & textures of the meshes all the way to render the pixels in the render
  *  targets.
  *
  *  rough states:
  *  Vertex/index buffer -> Input Assembler -> Vertex shader
  *  ->Tessellation -> Geometry shader -> Rasterization
  *  -> Fragment shader -> Color blending -> Frame buffer
  **/
    // clang-format on
    void createGraphicsPipeline()
    {
        auto vertShaderCode = readFile("shaders/vert.spv");
        auto fragShaderCode = readFile("shaders/frag.spv");

        // compilation & linking from SPIR-V bytecode to machinecode will not
        // happen until the graphic pipeline is created so we need local
        // variables ... (glslc INPUTCODE.vert -o vert.spv)
        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType
            = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        // entrypoint - function
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType
            = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        // entrypoint - function
        fragShaderStageInfo.pName = "main";
        // an array which holds these shaderStructs
        VkPipelineShaderStageCreateInfo shaderStages[]
            = {fragShaderStageInfo, vertShaderStageInfo};

        // this will cause the configuration of these values to be ignored & you
        // need to specify the data at drawing time this results in a more
        // flexible setup (valuescan be made dynamic or static), well go dynamic
        // as its more flexible
        std::vector<VkDynamicState> dynamicStates
            = {VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType
            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount
            = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // with commit 019 we introduce vertex shader data
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions
            = &bindingDescription; // optional
        vertexInputInfo.vertexAttributeDescriptionCount
            = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions
            = attributeDescriptions.data(); // optional

        // InputAssembly: describes 2 things:
        // a) what kind of geometry will be drawn, topology members
        // b) if primitive restart should be enabled
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType
            = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        // normally vertices are loaded from the vertex buffer by index
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // when setting to true the its possible to breakup lines
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // setup viewport: it describes the region of the framebuffer that the
        // output will be rendered to mostly this is [(0,0) to (width, height)]

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        // specify the range of depth values to use for the framebuffer
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // scissor rectangles define in which regions pixels will be stored
        // pixels outside this rectangle will be discarded
        // can also be larger than the viewport and acts like a filter rather
        // than a transformation
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;

        // the specify their count at pipeline creation time
        // withoutr dynamic state they need to be set in the pipeline and make
        // the pipeline immutable, any changes require a new pipeline
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType
            = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        // Rastorizer takes the geometry that is shaped by the vertices from the
        // vertex shader & turns it into fragments to be colored by fragment
        // shader it also performs deep testing, face culling, scissor test &
        // can be figured to output fragments that fill entire polygons or
        // justthe edges
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType
            = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable
            = VK_FALSE; /// if enabled fragments that are beyond the near & far
                        /// planes are clamped to them (useful for shadowmaps &
                        /// enables a GPU feature)
        rasterizer.rasterizerDiscardEnable
            = VK_FALSE; /// if true geomtry never passes through the rasterizer
                        /// stage, basically disables output to the framebuffer

        rasterizer.polygonMode
            = VK_POLYGON_MODE_FILL; /// determines how  fragments are generated
                                    /// for geometry, other than fill requires
                                    /// GPU-feature
        rasterizer.lineWidth
            = 1.0f; /// thickness of lines in terms of fragments, thicker lines
                    /// requires the "wideLines-GPU-feature"
        rasterizer.cullMode
            = VK_CULL_MODE_BACK_BIT; /// type of faceculling to use
        rasterizer.frontFace
            = VK_FRONT_FACE_COUNTER_CLOCKWISE; /// specifies the vertex order
                                               /// for faces to be considered
        rasterizer.depthBiasEnable
            = VK_FALSE; /// used for shadow mapping, wont use it for now
        rasterizer.depthBiasConstantFactor = 0.0f; /// optional
        rasterizer.depthBiasClamp = 0.0f;          /// optional
        rasterizer.depthBiasSlopeFactor = 0.0f;    /// optional

        // Multisampling is one way to perform anti-aliasing
        // works by combining fragment shader results of multiple polygons that
        // rasterize the same pixel mainly occurs along edges, its less
        // expensive than rendering to a higher resolution & then downscaling.
        // enabling requires a GPU-feature, disabled for now
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType
            = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;          // Optional
        multisampling.pSampleMask = nullptr;            // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE;      // Optional

        // if using depth/stencil buffer one need to fill the
        // VkPipelineDepthStencilStateCreateInfo struct, for now nullptr

        // after a fragment has returned a color it needs to be combined with
        // the color that is already in the framebuffer, this is known as
        // ColorBlending two structs are needed to be filled the transformation
        // can be done in 2 ways: a) Mix the old and new value to produce a
        // final color b) Combine the old and new value using a bitwise
        // operation we also disable both modes for now
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask
            = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
              | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable
            = VK_FALSE; // new color from the fragment shader is passed through
                        // unmodified
        colorBlendAttachment.srcColorBlendFactor
            = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor
            = VK_BLEND_FACTOR_ZERO;                          // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor
            = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor
            = VK_BLEND_FACTOR_ZERO;                          // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType
            = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // one can use uniform values in shaders, (= globals similar to dynamic
        // state variables that can be changed at drawing time to alter the
        // behavior of the shaders without having to recreate them. commonly
        // used to pass the transformation matrix to the vertex shader, or to
        // create texture samplers in the fragment shader. These uniform values
        // need to be specified during pipeline creation by creating a
        // VkPipelineLayout object. see createDescriptorSetLayout()
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType
            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;                 // Optional
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0;         // Optional
        pipelineLayoutInfo.pPushConstantRanges
            = nullptr; // Optional, another way of passing dynamic values to
                       // shaders

        if (vkCreatePipelineLayout(
                device, &pipelineLayoutInfo, nullptr, &pipelineLayout)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // referencing the array of VkPipelineShaderStageCreateInfo-structs
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        // referencing the fixed-function stage
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; /// optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        // pipelineLayout
        pipelineInfo.layout = pipelineLayout;
        // renderpass reference
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        // Vulkan allows you to create a new graphics pipeline from an existing
        // one (less expensove) at the moment we only have a single pipeline, so
        // we specify a nullHandle and invalid index only used if
        // VK_PIPELINE_CREATE_DERIVATIVE_BIT is also specified
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1;              // Optional

        // final step !
        // VkPipelineCache can be used to store & reuse data relevant to
        // pipeline creation across multiple calls to vkCreateGraphicsPipeline
        //
        if (vkCreateGraphicsPipelines(device,
                                      VK_NULL_HANDLE,
                                      1,
                                      &pipelineInfo,
                                      nullptr,
                                      &graphicsPipeline)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        //.. and need to cleanup the shaderModules here ...
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    /**
     * A Framebuffer object references all VkImageView objetcs that represent
     * attachments
     * We only have atm a colorAttachment
     * the image which is used depends on which image the swap chain returns
     * when we retrieve one for representation We have to create a framebuffer
     * for all the images in the swap chain and use the one that corresponds to
     * the retrieved image at drawing time.
     */
    void createFrameBuffers()
    {
        // size it to the hold imageViews
        swapChainFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {swapChainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            // specifiy which renderpass the framebuffer needs to be compatible
            // with they need to use the same number and types of attachments
            framebufferInfo.renderPass = renderPass;
            // specify the VkImageView objects
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device,
                                    &framebufferInfo,
                                    nullptr,
                                    &swapChainFramebuffers[i])
                != VK_SUCCESS)
            {
                throw std::runtime_error("failed tp create framebuffer");
            }
        }
    }

    /**
     * Command pools are opaque objects that command buffer memory is allocated
     * from, and which allow the implementation to amortize the cost of resource
     * creation across multiple command buffers.
     * */
    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices
            = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags
            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; /// allow command
                                                               /// buffers to be
                                                               /// rerecorded
                                                               /// individually,
                                                               /// without they
                                                               /// have to be
                                                               /// reset
                                                               /// together
        // command buffers are executed by submitting them on one of the device
        // queues each command can only allocate command buffers that are
        // submitted on a single type of queue we chose commands for drawing so
        // graphics queue family is chosen
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    /**
     * Although we could set up the shader to access the pixel values in the
     * buffer, it’s better to use image objects in Vulkan for this purpose.
     * Image objects will make it easier and faster to retrieve colors by
     * allowing us to use 2D coordinates, for one. Pixels within an image object
     * are known as texels
     * */
    void createTextureImage()
    {
        int texWidth, texHeight, texChannels;
        stbi_uc *pixels = stbi_load("textures/texture.jpg",
                                    &texWidth,
                                    &texHeight,
                                    &texChannels,
                                    STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(imageSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer,
                     stagingBufferMemory);

        // we directly copy the pixel values from the image loading library to
        // the buffer
        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        // clean up original pixel array
        stbi_image_free(pixels);

        createImage(texWidth,
                    texHeight,
                    VK_FORMAT_R8G8B8A8_SRGB,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT
                        | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    textureImage,
                    textureImageMemory);
        // to copy the staging buffer to the texture image two steps are needed
        // 1. Transition the texture image to
        //    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        // 2. Execute the buffer to image
        //    copy operation
        transitionImageLayout(textureImage,
                              VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        copyBufferToImage(stagingBuffer,
                          textureImage,
                          static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));

        // prepare it for shader access
        transitionImageLayout(textureImage,
                              VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    // as more images wwill be created we abstract the image creation
    void createImage(uint32_t textureWidth,
                     uint32_t textureHeight,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties,
                     VkImage &image,
                     VkDeviceMemory &imageMemory)
    {

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType
            = VK_IMAGE_TYPE_2D; /// tells vulkan which kind of coordinate system
                                /// the texels in the image are going to use
        imageInfo.extent.width
            = textureWidth; /// specifies the dimensions of the immage
        imageInfo.extent.height = textureHeight;
        imageInfo.extent.depth = 1; /// how many texels are on each axis
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format; /// should use the same format for the
                                   /// texels as the pixels in the buffer,
                                   /// otherwise the copy operation will
                                   /// fail.
        imageInfo.tiling = tiling; /// tiling mode can't be
                                   /// changed at a later time
        imageInfo.initialLayout
            = VK_IMAGE_LAYOUT_UNDEFINED; /// Not usable by the GPU and the very
                                         /// first transition will discard the
                                         /// texels.
        imageInfo.usage = usage;
        /// dst for bufferccopy & access the
        /// image from the shader to color
        /// the mesh
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0; /// Optional, multisampling related

        if (vkCreateImage(device, &imageInfo, nullptr, &textureImage)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image!");
        }

        // allocating memory for an image works exactly the same way as
        // allocation memory for a buffer, allthough requirements and
        // bind-methods differ
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex
            = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    /**
     * Images are accessed through image views rather than directly
     * */
    void createTextureImageView()
    {
        // almmost the same as createImageViews() except format and image
        textureImageView
            = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB);
    }

    /**
     * Textures are usually accessed through samplers, which will apply
     * filtering and transformations to compute the final color that is
     * retrieved. These filters are helpful to deal with problems like
     * oversampling.
     * */
    void createTextureSampler()
    {
        VkSamplerCreateInfo
            samplerInfo{}; /// specifies all filters & transformations that it
                           /// should apply
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter
            = VK_FILTER_LINEAR; /// how to interpolate texels that are magnified
                                /// or minified
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU
            = VK_SAMPLER_ADDRESS_MODE_REPEAT; /// mode for texturespace
                                              /// coordinate for x
        samplerInfo.addressModeV
            = VK_SAMPLER_ADDRESS_MODE_REPEAT; /// mode for texturespace
                                              /// coordinate for y
        samplerInfo.addressModeW
            = VK_SAMPLER_ADDRESS_MODE_REPEAT; /// mode for texturespace
        /// coordinate for z

        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;

        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        samplerInfo.maxAnisotropy
            = properties.limits.maxSamplerAnisotropy; /// max quality
        samplerInfo.borderColor
            = VK_BORDER_COLOR_INT_OPAQUE_BLACK; /// no arbitrary color allowed
        samplerInfo.unnormalizedCoordinates
            = VK_FALSE; /// which coordinate system to use to adress texels in
                        /// an image, TRUE: [0,texWidth / 0,texHeight], FALSE:
                        /// [0,1) = normmalilzed coordinates

        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        // Mipmapping related
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level
            = VK_COMMAND_BUFFER_LEVEL_PRIMARY; /// Can be submitted to a queue
                                               /// for execution, but cannot be
                                               /// called from other command
                                               /// buffers.
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data())
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSyncObjects()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags
            = VK_FENCE_CREATE_SIGNALED_BIT; /// first call to vkWaitForFence()
                                            /// returns immediately instead of
                                            /// waiting for previous frames
                                            /// (which do not exist at the
                                            /// beginning)

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(device,
                                  &semaphoreInfo,
                                  nullptr,
                                  &imageAvailableSemaphores[i])
                    != VK_SUCCESS
                || vkCreateSemaphore(device,
                                     &semaphoreInfo,
                                     nullptr,
                                     &renderFinishedSemaphores[i])
                       != VK_SUCCESS
                || vkCreateFence(
                       device, &fenceInfo, nullptr, &inFlightFences[i])
                       != VK_SUCCESS)
            {
                throw std::runtime_error(
                    "failed to create synchronization objects for a frame!");
            }
        }
    }

    /**
     * It is possible for the window surface to change such that the swap chain
     * is no longer compatible with it. One of the reasons that could cause this
     * to happen is the size of the window changing.
     * */
    void recreateSwapChain()
    {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        while (width == 0 || height == 0)
        { /// when window is minimized we pause
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device); /// to make sure that we don't touch
                                  /// ressources that are still in use

        cleanUpSwapChain();

        createSwapChain();
        createImageViews();
        createFrameBuffers(); /// recreating the renderpass would only be
                              /// needable if the swap chain image format is
                              /// changing (e.g. moving from a standard to a HDR
                              /// monitor)
    }
    /**
     * function that writes the commands we want to execute into a command
     *buffer.
     **/
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; /// dont use atm                  /// optional
        beginInfo.pInheritanceInfo
            = nullptr; /// optional only for secondary command buffers

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error(
                "failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        // size of the renderArea, defines where shader loads and stors will
        // take place, pixel outside this area will have undefined values
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1; /// clear values
        renderPassInfo.pClearValues
            = &clearColor; /// for VK_ATTACHMENT_LOAD_OP_CLEAR

        vkCmdBeginRenderPass(
            commandBuffer,
            &renderPassInfo,
            VK_SUBPASS_CONTENTS_INLINE); /// render pass commands will be
                                         /// embedded in the primary command
                                         /// buffer

        vkCmdBindPipeline(
            commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        VkBuffer vertexBuffers[] = {vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer,
                               0,
                               1,
                               vertexBuffers,
                               offsets); /// bind vertex buffers to bindings
        // only possible to have  a single index buffer
        // not possible to use different indices for each vertex attribute (if
        // one attribute varies we still have to duplicate vertex data)
        vkCmdBindIndexBuffer(
            commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        // as viewport and scissor state is dynamic we need to set them in
        // cmmand buffer before drawing

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent.width);
        viewport.height = static_cast<float>(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        // Unlike vertex and index buffers, descriptor sets are not unique to
        // graphics pipelines.
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0,
                                1,
                                &descriptorSets[currentFrame],
                                0,
                                nullptr);

        // vertexCount = size of vertices-list, instanceCount = 1 (for instanced
        // rendering), firstVertex: used as an offest into the vertex buffer
        // defines the lowest value of gl_VertexIndex firstInstance:Used as an
        // offset for instanced rendering, defines the lowest value of
        // gl_InstanceIndex
        // vkCmdDraw(
        //    commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

        // when using an indexbuffer this is the method to draw stuff
        vkCmdDrawIndexed(
            commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer");
        }
    }

    void createBuffer(VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory)
    {

        // buffers in Vulkan are regions of memory used for storing arbitrary
        // data that can be read by the GPU. They can be used to store vertex
        // data. buffers do not automatically allocate memory
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create vertex buffer");
        }

        // buffer created but no memory allocated for it
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements
                .memoryTypeBits, // VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            properties);

        // for a large number of allocations (or a real
        // world app) its good practice to create a
        // custom memory allocator or use:
        // https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
        // the maximum number of allocation is also
        // limited ba "maxMemoryAllocationCount"
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory)
            != VK_SUCCESS)
        {
            throw std::runtime_error(
                "failed to allocate vertex buffer memory!");
        }
        // if memory allocation was succesful then we can associate the memory
        vkBindBufferMemory(device,
                           buffer,
                           bufferMemory,
                           0); /// last parameter defines a offset within the
                               /// region of memory, if
                               /// !=0 it is required to be devisible by
                               /// memRequirements.alignment
    }

    /**
     * Vulkan API puts the programmer in control of almost everything, also
     * memory menagement we need to handle this if the vertex data is managed by
     * our app and not by the shader itself (which is not good)
     * */
    void createVertexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,   /// can be used as a source
                                                /// in a memory transfer
                                                /// operation
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT /// Use a memory heap that is
                                                /// host coherent,
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory);
        // copy the vertex data to the buffer by mapping the buffermemory into
        // CPU accessible memor
        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data,
               vertices.data(),
               (size_t)
                   bufferSize); /// Unfortunately the driver may not immediately
                                /// copy the data into the buffer memory, for
                                /// example because of caching. It is also
                                /// possible that writes to the buffer are not
                                /// visible in the mapped memory yet. There are
                                /// two ways to deal with that problem:
        vkUnmapMemory(device, stagingBufferMemory);

        // memory is allocated from a memory type that is device local (not able
        // to use vkMapMemory)
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT /// can be used as a destination
                                             /// in
                                             /// a memory operation
                | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBuffer,
            vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

        // clean staging buffer
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    /**
     * Almost the same as VertexBuffer creation, but size is sizeof(indices) and
     * we're using the VK_BUFFER_USAGE_INDEX_BUFFER_BIT flag to create the
     * buffer
     * */
    void createIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     stagingBuffer,
                     stagingBufferMemory);

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t)bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize,
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     indexBuffer,
                     indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            createBuffer(bufferSize,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         uniformBuffers[i],
                         uniformBuffersMemory[i]);
            // We map the buffer right after creation using vkMapMemory to get a
            // pointer to which we can write the data later on. The buffer stays
            // mapped to this pointer for the application’s whole lifetime. This
            // technique is called "persistent mapping" and works on all Vulkan
            // implementations. Not having to map the buffer every time we need
            // to update it increases performances, as mapping is not free.
            vkMapMemory(device,
                        uniformBuffersMemory[i],
                        0,
                        bufferSize,
                        0,
                        &uniformBuffersMapped[i]);
        }
    }

    /**
     * Descriptor sets can’t be created directly, they must be allocated from a
     * pool like command buffers. Take care: inadequate descriptor pools are a
     * good example that the validation layer will not catch
     * */
    void createDescriptorPool()
    {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount
            = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount
            = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        // We will allocate one of these descriptors for every frame.

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets()
    {
        // In our case we will create one descriptor set for each frame in
        // flight, all with the same layout. Unfortunately we do need all the
        // copies of the layout because the next function expects an array
        // matching the number of sets.
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
                                                   descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount
            = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();
        descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data())
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }
        // now as they are allocated they need to be configured
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            // bind the image and sampler ressources to the descriptor in the
            // descriptor set, ressources for a combined image sampler must be
            // specified here
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = textureImageView;
            imageInfo.sampler = textureSampler;

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = descriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType
                = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo
                = &bufferInfo; /// used for descriptors that refer to buffer
                               /// data

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = descriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType
                = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo
                = &imageInfo; // Optional, used for descriptors that refer to
                              // image data
            descriptorWrites[1].pTexelBufferView
                = nullptr; // Optional, used for descriptors that refer to
                           // buffer views

            vkUpdateDescriptorSets( /// descriptors are now ready to be used in
                                    /// the shaders
                device,
                static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(),
                0,
                nullptr);
        }
    }

    /**
     * Memory tranfer operations are executed using command buffers (like
     * drawing commands)
     * */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer,
                           VkImage image,
                           uint32_t width,
                           uint32_t height)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        // specify which part of the buffer is going to be copied to which part
        // of the image
        VkBufferImageCopy region{};
        region.bufferOffset
            = 0; /// byteoffset in buffer at which the pixel values start
        region.bufferRowLength
            = 0; ///  specify how the pixels are laid out in memory.
        region.bufferImageHeight = 0;
        // to which part we want to copy the pixels
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, /// which layout the image is
                                                  /// currently using
            1,
            &region);

        endSingleTimeCommands(commandBuffer);
    }
    /**
     * Helper function to find correct memory type on graphic card for
     * allocation Each type of mem varies in terms of allowed operation &
     * performance characteristica
     *
     * @param typeFilter  specify the bit field of memory types that are
     * suitable.
     * @param properties  define special features of the memory, like being
     * able to map it so we can write to it from the CPU
     * */
    uint32_t findMemoryType(uint32_t typeFilter,
                            VkMemoryPropertyFlags properties)
    {
        // query  infos about abeilable types of memory
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i))
                && (memProperties.memoryTypes[i].propertyFlags & properties)
                       == properties)
            {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    /*
    ** Helper function to wrap a shaderBuffer to a VkShaderModule object
    */
    VkShaderModule createShaderModule(const std::vector<char> &code)
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void populateDebugMessengerCreateInfo(
        VkDebugUtilsMessengerCreateInfoEXT &createInfo)
    {

        createInfo = {};
        createInfo.sType
            = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = /// sepcify all types of severity to
                                     /// let tha callback be called for
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType
            = /// which types of messages the callback is notified for
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback
            = debugCallback; /// pointer to the callback function
    }

    /**
     * Handles layout transitions, as vkCmmdCopyBufferToImage requires the
     * image to be in the right layout first
     * */
    void transitionImageLayout(VkImage image,
                               VkFormat format,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        // One of the most common ways to perform layout transitions is using an
        // image memory barrier.
        // a barrier is commonly used to sync access to resources (e.g.
        // ensurring that a write to a buffer completes before reading from it )
        // but can also be used to transition image layouts & transfer queue
        // family ownership when VK_SHARING_MODE_EXCLUSIVE is used

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        // If you are using the barrier to transfer queue family ownership, then
        // these two fields should be the indices of the queue families.
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        // which types of operations that involve the resource
        // must happen before the barrier and which op must wait on the
        // barrier
        // two transitions:
        // 1. Undefined → transfer destination: transfer writes that don’t need
        //    to wait on anything
        // 2. Transfer destination → shader reading: shader reads should wait on
        //    transfer writes, specifically the shader reads in the fragment
        //    shader, because that’s where we’re going to use the texture

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                   && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage
                = VK_PIPELINE_STAGE_TRANSFER_BIT; /// more like a pseudostage
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(commandBuffer,
                             sourceStage,
                             destinationStage,
                             0,
                             0,
                             nullptr,
                             0,
                             nullptr,
                             1,
                             &barrier);

        endSingleTimeCommands(commandBuffer);
    }

    /**
     * Helper function to fill commandBuffer
     * */
    VkCommandBuffer beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void setupDebugMessenger()
    {
        if (not enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        // requires a valid instance have been created -> to validate all
        // code before this we can use a sepatare debug utils messenger
        if (CreateDebugUtilsMessengerEXT(
                instance, &createInfo, nullptr, &debugMesseger)
            != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    // logical device to interface with
    // could setup more logical device from one physical device for
    // different requirements
    VkDevice device;
    // queues are automatically created with the logical device but we need
    // a handle to interface with, the are implicitly cleaned up with the
    // device
    VkQueue graphicsQueue;
    VkSurfaceKHR surface;
    VkQueue presentQueue;
    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    // to use any VkImage, we need a view into a image, it described how to
    // access the image and which part of the image, should be treated as 2D
    // depth texture wothout any mimapping levels
    std::vector<VkImageView> swapChainImageViews;
    VkDescriptorSetLayout descriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    std::vector<VkFramebuffer> swapChainFramebuffers; /// holds all framebuffers

    VkCommandPool commandPool; /// manages the memory that is used to store
                               /// the buffers and command buffers which are
                               /// allocated from them
    std::vector<VkCommandBuffer>
        commandBuffers; /// with frames in flight (concurrent processed
                        /// ones) each frame needs its own set of
                        /// commandbuffers & semaphores & fences
    VkDebugUtilsMessengerEXT debugMesseger;
    GLFWwindow *window;
    VkInstance instance;
    // synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    uint32_t currentFrame = 0;

    bool framebufferResized = false;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    // We should have multiple buffers, because multiple frames may be in
    // flight at the same time and we don’t want to update the buffer in
    // preparation of the next frame while a previous one is still reading
    // from it! Thus, we need to have as many uniform buffers as we have
    // frames in flight, and write to a uniform buffer that is not currently
    // being read by the GPU.
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView textureImageView;
    VkSampler textureSampler;
};

int
main()
{

    TriangleApp app;
    try
    {
        app.run();
    } catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
