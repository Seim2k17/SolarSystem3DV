#include <GLFW/glfw3native.h>
#include <bits/stdint-uintn.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

#ifdef WIN32
#include <vulkan/vulkan_win32.h>
#define VK_USE_PLATFORM_WIN32_KHR /// Platform windows support ...
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

const uint32_t WINDOW_WIDTH = 800;
const uint32_t WINDOW_HEIGHT = 600;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

// a limited amount of states can be changed without recreating the pipeline at
// draw time
const std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                   VK_DYNAMIC_STATE_SCISSOR};

// clang-format off
// check if its a debug-build or not
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif
// clang-format on

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  // extension function is not automatically loaded, tf look up its adress
  // will return nullptr if the function couldn't be loaded
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

/*
** Helper function to open the shader files
*/
static std::vector<char> readFile(const std::string &filename) {
  // start at the end of the file (ate) / binary
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (not file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  // bc of reading at the end we can determine the size of the file and allocate
  // a buffer
  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);
  std::cout << "read bufferSize: " << fileSize << std::endl;

  // the seek back and read data at once
  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

struct QueueFamilyIndices {
  // graphicsFamily could have a value or not
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
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

class TriangleApp {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private:
  bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // check if all defined validationlayers exists in the availableLayersVector
    for (const char *layerName : validationLayers) {
      bool layerFound = false;

      for (const auto &layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
          layerFound = true;
          break;
        }
      }

      if (not layerFound) {
        return false;
      }
    }

    return true;
  }

  /* Vulkan does not have the concept of a default framebuffer, it requires an
   * infrastructure that will own the buffers we will render to, before
   * visualizing them on the screen. This is known as the swap chain and must be
   * explicitly created in vulkan. Not all graphic cards are capable of
   * presenting images directly to a screen. Image prsentation is heavily tied
   * into the window system & the surfaces associated with windows , its not
   * part of Vulkan. We have to enable VK_KHR_swapchain device extension
   *
   *
   * */
  bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    // check if all required extensions are available
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                         availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                             deviceExtensions.end());

    for (const auto &extension : availableExtensions) {
      requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
  }

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); /// do not a OpenGL context
    glfwWindowHint(GLFW_RESIZABLE,
                   GLFW_FALSE); /// disable resizing of windows for now
    window = glfwCreateWindow(
        WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Tutorial", nullptr,
        nullptr); /// optionally specify a monitor to open the window on,
                  /// last parameter relevant to OpenGL
  }

  void initVulkan() {
    createInstance();
    setupDebugMessenger();
    setupWindowSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
  }

  void mainLoop() {
    while (not glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }

    // as all operations are async in drwaFrame() & when exiting the mainLoop,
    // drawing amy still be going on, cleaning things up while drawing is a bad
    // idea
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
  void drawFrame() {
    // synchronization of execution on the GPU is explicit
    // the order of operations is up to us
    // many Vulkan API calls are asychronous
    // these calls will return before the operations are actually finished
    // & the order of execution is also undefined
    // each of the operations depends on the previous one finishing
    // using a semaphore to add order between queue operations
    // semaphore are used to order work inside the same & different queues
    // there two kinds of semaphores in Vulkan: binary & timeline, we use binary
    // here its either unsignaled or signaled we use it as signal semaphore in
    // one queue operation & as a wait operation in another queue operation, for
    // ordering execution on the CPU we use a fence as a similar mechanism
    // -> if the host needs to know when the GPU has finished something we use a
    // fence
    // -> semaphores are used to specify the execution of order of operations on
    // the GPU
    // -> fences are used to keep the CPU&GPU in sync with each other. We want
    // to use semaphores for swapchain operations (they happen on the GPU) for
    // waiting on the previous frame to finish we want to use fences, we need
    // the host to wait (CPU)
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE,
                    UINT64_MAX); /// disabled wit UINT64_MAX the timeout

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                          imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    // with the imageIndex spec. the swapchain image we can now record the
    // command buffer
    //
    vkResetCommandBuffer(commandBuffer, 0);

    recordCommandBuffer(commandBuffer, imageIndex);
    // queue submission of the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // for details see Tutorial: submitting the command buffer
    // which semaphore to wait on before the execution begins & in which stages
    // the pipeline to wait
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    // which command buffer to submit for execution
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    // which semaphores to signal once the command buffer(s) have finished
    // execution
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to submit draw command buffer");
    }

    // last step to draw is submitting the result back to the swap chain to show
    // it on the screen
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
    vkQueuePresentKHR(presentQueue, &presentInfo);
  }

  void cleanup() {
    // must be destroyed before the instance -> to validate all code after this
    // we can use a sepatare debug utils messenger
    if (enableValidationLayers) {
      DestroyDebugUtilsMessengerEXT(instance, debugMesseger, nullptr);
    }
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    for (auto framebuffer : swapChainFramebuffers) {
      vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(device, renderPass, nullptr);
    for (auto imageView : swapChainImageViews) {
      vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    // destroy the instance right before the window
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface,
                        nullptr); /// surface need to be destroyed before the
                                  /// instance destruction !
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    std::cout << "Cleanup!" << std::endl;
    glfwTerminate();
  }

  /**
   * There is no global state in Vulkan and all per-application state is stored
   * in a VkInstance object. Creating a VkInstance object initializes the Vulkan
   * library and allows the application to pass information about itself to the
   * implementation.
   * */
  void createInstance() {
    // 0. check for validationLayers (debugging)
    if (enableValidationLayers && not checkValidationLayerSupport()) {
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

    // GLFW has built-in function that returns the extensionscount, for standard
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtension;
    glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(
        extensions.size()); // standard: glfwExtensionCount;
    createInfo.ppEnabledExtensionNames =
        extensions.data(); // stadnard: glfwExtension;
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    // global validation layer determination, only available in debug build
    if (enableValidationLayers) {
      createInfo.enabledLayerCount =
          static_cast<uint32_t>(validationLayers.size());
      createInfo.ppEnabledLayerNames = validationLayers.data();

      populateDebugMessengerCreateInfo(debugCreateInfo);
      debugCreateInfo.pNext =
          (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
    } else {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
    }

    // could check for extension support (see instance page bottom)
    // ...
    //
    // 3. finally create the instance and check result
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      throw std::runtime_error("failed to create instance");
    }
  }

  // for setting up a callback to handle messages and details for the validation
  // layer
  std::vector<const char *> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions,
                                         glfwExtensions + glfwExtensionCount);
    if (enableValidationLayers) {
      extensions.push_back(
          VK_EXT_DEBUG_UTILS_EXTENSION_NAME); // to avoid typos we use the macro
    }

    return extensions;
  }

  // debug callback, VKAPI_ATTR / VKAPI_CALL ensure the right signature for
  // Vulkan
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT
                    messageSeverity, /// Diag, Info, prop. Bug, Invalid
                VkDebugUtilsMessageTypeFlagsEXT
                    messageType, /// unrelated to spec or perf., spec-violation,
                                 /// non-optimal Vulkan-use
                const VkDebugUtilsMessengerCallbackDataEXT
                    *pCallbackData, /// details of the message
                void *pUserData)    /// pass own userdata
  {
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE; /// if true the call is aborted with
                     /// VK_ERROR_VALIDATION_FAILED_EXT
  }

  // window need to be setup right after the instance creation, it can influence
  // the physical device selection window surfaces are optional component of
  // vulkan (e.g. one need off-screen rendering)
  void setupWindowSurface() {
#ifdef WIN32
    // Windows specific code:
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = glfwGetWin32Window(window);
    createInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create window surface!");
    }
#endif

#ifdef __linux__
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create window!");
    }
#endif
  }

  // before drawing, even before almost every operation in Vulkan commands need
  // to be submitted to a queue, there are differet types of queues that are
  // originated in different queue families, each family only allows processing
  // of certain commands we need to check which queue families are supported by
  // the device for now we only looking for a queu that supports graphic
  // commands
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    // logic to find graphics queue family
    QueueFamilyIndices indices;
    // 1. get the queueFamilyCount
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             nullptr);

    // 2. then get the properties in the exactly length
    // VkQueueFamilyProperties - struct contains details about the queue family,
    // incl. type of operations that are supported

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                             queueFamilies.data());

    // for now we need a queue that supports >VK_QUEUE_GRAPHICS_BIT

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

      // bc it does not mean that every every device support the window system
      // integration so we need to find a queue-family that support presenting
      // to the surface, it's very likely that these end up being the same queue
      // family after all
      if (presentSupport) {
        indices.presentFamily = i;
      }
      if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i; // 1;
      }
      // early return we found the needed capability
      if (indices.isComplete()) {
        break;
      }
      ++i;
    }
    return indices;
  }

  int rateDeviceSuitability(VkPhysicalDevice device) {
    int score = 0;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      score += 1000;
    }
    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // Application can't function without geometry shaders
    if (!deviceFeatures.geometryShader) {
      return 0;
    }

    return score;
  }

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;
    // 1. basic surface capibilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                              &details.capabilities);

    // 2, surface formats (pixel format, color space)
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         nullptr);

    if (formatCount != 0) {
      // make sure that the vector can hold all formats
      details.formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                           details.formats.data());
    }

    // 3. available presentation modes
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount,
                                              nullptr);

    if (modeCount != 0) {
      // make sure that the evctor can hold all formats
      details.presentModes.resize(modeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &modeCount,
                                                details.presentModes.data());
    }

    return details;
  }

  // determine the surfaceformat (color depth, color channel, colortype,
  // colorspace) BGRA
  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR> &availabeFormats) {
    // check if the preferred combination is available
    for (const auto &availableFormat : availabeFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }

    // If that also fails then we could start ranking the available formats
    // based on how "good" they are, but in most cases it’s okay to just settle
    // with the first format that is specified.
    return availabeFormats[0];
  }

  /*
  ** Presentation mode is the most important setting in the swap chain, it
  *  represents the actual conditions for showing images to the screen
  ** There are four possible modes available in Vulkan
  ** 1. VK_PRESENT_MODE_IMMEDIATE_KHR: images are submitted to the screen right
  *     away (may result in tearing)
  ** 2. VK_PRESENT_MODE_FIFO_KHR: swap chain is a queue, display takes an image
  *     from the front of the queue when the display is refreshed & the program
  *     inserts rendered images at the back of the queue
  **    if the queue is full then the prgram has to wait. Its similar to VSYNC.
  *     the moment that the display is refreshed is known as "vertical blank"
  ** 3. VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from 2. if the
  *     app is late and the queue was empty at the last vertical blank.
  **    Instead of waiting for the next vertical blank, the image is transferred
  *     right away when it finally arrives. This may result in visible tearing.
  ** 4. VK_PRESENT_MODE_MAILBOX_KHR: variation of 2. Instead of blocking the app
  *     when the queue is full, the images that are already queued are simply
  *     replaced with newer ones.
  **    This mode can be used to render frames as fast as possible while still
  *     avoiding tearing, resulting in fewer latency issues than standard
  *     vertical sync. Commonly known as "triple buffering", although three
  *     buffers alone does not necessarily mean that the framerate is unlocked.
  */
  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR> &availablePresentModes) {
    // VK_PRESENT_MODE_MAILBOX_KHR is a nice tradeoff if energy usage is not a
    // concern on mobile where energy usage is more important we want to use
    // VK_PRESENT_MODE_FIFO_KHR
    for (const auto &availablePresentMode : availablePresentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }

    // this value is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  /*
  ** swap extend is the resolution of the swap chain images, almost always equal
  *to the resolution of the window that we're drawing in pixels
  */
  VkExtent2D chooseSwapExtend(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width !=
        std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtend = {static_cast<uint32_t>(width),
                                 static_cast<uint32_t>(height)};

      actualExtend.width =
          std::clamp(actualExtend.width, capabilities.minImageExtent.width,
                     capabilities.minImageExtent.width);
      actualExtend.height =
          std::clamp(actualExtend.height, capabilities.minImageExtent.height,
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
  void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
      throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    // rate the physical device and pick the one with the highest score (which
    // def. meet the requirement)
    std::multimap<int, VkPhysicalDevice> candidates;

    for (const auto &device : devices) {
      int score = rateDeviceSuitability(device);
      candidates.insert(std::make_pair(score, device));
    }
    if (candidates.begin()->first > 0) {
      physicalDevice = candidates.rbegin()->second;
    } else {
      throw std::runtime_error("failed to find a suitable GPU!");
    }
  }

  // check GPU for vulkan-features we need for our app, later on we add more !
  bool isDeviceSuitable(VkPhysicalDevice device) {
    /* this could be a device selection process
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    std::cout << "DeviceName: " << deviceProperties.deviceName << std::endl;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    // e.g. app only usable for dedicated graphics cards that support geometry
    // shader
    return deviceProperties.deviceType ==
               VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
           deviceFeatures.geometryShader;
    */
    // ensure that the device can process the commands we want to use
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
      SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
      swapChainAdequate = not swapChainSupport.formats.empty() &&
                          not swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
  }

  void createLogicalDevice() {
    // 1. specifying details in structs
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                              indices.presentFamily.value()};

    // influence scheduling of command buffer execution from 0.0 .. 1.0
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
      queueCreateInfo.queueCount = 1;
      queueCreateInfo.pQueuePriorities = &queuePriority;
      queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;
    // using the swapchain : enabling the VK_KHR_swapchain
    createInfo.enabledExtensionCount =
        static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    /* no device specific extension needed for now */
    /*
    createInfo.enabledExtensionCount;
    createInfo.ppEnabledExtensionNames;
    createInfo.flags;
    */
    if (enableValidationLayers) {
      createInfo.enabledLayerCount =
          static_cast<uint32_t>(validationLayers.size());
      createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
      createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create logical device!");
    } else {
      std::cout << "Logical device: " << device << " created." << std::endl;
    }
    // as we only create a single queue from this family, we use 0 as a index
    // retrieve the  queue handle
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
  }

  void createSwapChain() {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat =
        chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode =
        chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtend(swapChainSupport.capabilities);

    // how many images we want to have in the swap chain, but simply sticking to
    // this minimum means that we may sometimes have to wait on the driver to
    // complete internal operations before we can acquire another image to
    // render to. Therefore it is recommended to request at least one more image
    // than the minimum
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // we should also make sure to not exceed the maximum number of images while
    // doing this, where 0 is a special value that means that there is no
    // maximum:
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
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
    // this specifies what kind of operations we'll use the images in the swap
    // chain for (here used as color attachment bc we render directly to them)
    // It is also possible that you’ll render images to a separate image first
    // to perform operations like post-processing. In that case you may use a
    // value like VK_IMAGE_USAGE_TRANSFER_DST_BIT instead and use a memory
    // operation to transfer the rendered image to a swap chain image.
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // specify how to handle swap chain images that will be used across multiple
    // queue families. That will be the case in our application if the graphics
    // queue family is different from the presentation queue. We’ll be drawing
    // on the images in the swap chain from the graphics queue and then
    // submitting them on the presentation queue. There are two ways to handle
    // images that are accessed from multiple queues:
    // 1. VK_SHARING_MODE_EXCLUSIVE: An image is owned by one queue family at a
    // time and ownership must be explicitly transferred before using it in
    // another queue family. This option offers the best performance.
    // 2. VK_SHARING_MODE_CONCURRENT: Images can be used across multiple queue
    // families without explicit ownership transfers.

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                     indices.presentFamily.value()};

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0;     // Optional
      createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    // one can apply a certainTransform (rotate, flip ...)
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    // for blending with other windows, almost always ignore the alpha
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    // we dont care about the color of pixels that are obscured, better
    // performance if clipping is enabled
    createInfo.clipped = VK_TRUE;
    // eg if windows are resized, we need to build SwapChain from scratch more
    // on this later for now: its null
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain)) {
      throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount,
                            swapChainImages.data());
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
  }

  /*
  ** An imagevieview is sufficient to start using an image as a texture, but to
  *  be rendered a frambuffer is needed (see graphic-pipelines-creation)
  *
  *  An image view is quite literally a view into an image. It describes how to
  *  access the image and which part of the image to access, for example if it
  *  should be treated as a 2D texture depth texture without any mipmapping
  *  levels.
  *   */
  void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapChainImageFormat;
      // allow to swizzle the  color  channels around , e.g. map all channels to
      // the red channel for a monochrome texture: here we stick to the default
      // The swizzle can rearrange the components of the texel, or substitute
      // zero or one for any components. It is defined as follows for each color
      // component:
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      // describes the purpose of the image and which part of the image should
      // be accessed here our images are color targets without mipmapping levels
      // or multiple layers
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount =
          1; /// working with a stereographic 3D app, a swap chain with multiple
             /// layers is needed (VR?)

      if (vkCreateImageView(device, &createInfo, nullptr,
                            &swapChainImageViews[i]) !=
          VK_SUCCESS) { /// manual creation process demands a manual destroy !
        throw std::runtime_error("failed to create image views!");
      }
    }
  }

  /* Draw commands must be recorded within a render pass instance. Each render
   * pass instance defines a set of image resources, referred to as attachments,
   * used during rendering. Before creating the pipeline we need to tell Vulkan
   * about the framebuffer attachements that will be used while rendering.
   */
  void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat; /// should match the format
                                                   /// of the swap chain images
    colorAttachment.samples =
        VK_SAMPLE_COUNT_1_BIT; /// no multisampling, 1 sample
    colorAttachment.loadOp =
        VK_ATTACHMENT_LOAD_OP_CLEAR; /// clear framebuffer to black before
                                     /// drawing a new frame
    colorAttachment.storeOp =
        VK_ATTACHMENT_STORE_OP_STORE; /// rendered contents will be stored in
                                      /// memory and can be read later
    colorAttachment.stencilLoadOp =
        VK_ATTACHMENT_LOAD_OP_DONT_CARE; /// we wont do anything with stencil
                                         /// buffer
    colorAttachment.stencilStoreOp =
        VK_ATTACHMENT_STORE_OP_DONT_CARE; /// so results on load and store is
                                          /// irrelevant
    colorAttachment.initialLayout =
        VK_IMAGE_LAYOUT_UNDEFINED; /// we dont care what previous layout the
                                   /// image was in
    colorAttachment.finalLayout =
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; /// we want the img to be ready for
                                         /// presentation uisng the swap chain
                                         /// after rendering

    // every subpass references one or more attachments
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment =
        0; /// we ony have 1 VkAttachmentDescription, so index is 0
    colorAttachmentRef.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; /// attachment is intended as
                                                  /// a color buffer with "best
                                                  /// performance"

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.colorAttachmentCount = 1;

    // subpas dep. we need to ensure that the renderpasses don't begin until the
    // image is available
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    // sepcify the operations to wait on and stages in which these operations
    // occur
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

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create render pass!");
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
  void createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    // compilation & linking from SPIR-V bytecode to machinecode will not happen
    // until the graphic pipeline is created so we need local variables ...
    // (glslc INPUTCODE.vert -o vert.spv)
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    // entrypoint - function
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    // entrypoint - function
    fragShaderStageInfo.pName = "main";
    // an array which holds these shaderStructs
    VkPipelineShaderStageCreateInfo shaderStages[] = {fragShaderStageInfo,
                                                      vertShaderStageInfo};

    // this will cause the configuration of these values to be ignored & you
    // need to specify the data at drawing time this results in a more flexible
    // setup (valuescan be made dynamic or static), well go dynamic as its more
    // flexible
    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_SCISSOR,
                                                 VK_DYNAMIC_STATE_VIEWPORT};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount =
        static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // as we hard coded the vertex data directly in the vertex shader for now,
    // we specify that there is no vertex data to load for now
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // optional

    // InputAssembly: describes 2 things:
    // a) what kind of geometry will be drawn, topology members
    // b) if primitive restart should be enabled
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
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
    // can also be larger than the viewport and acts like a filter rather than a
    // transformation
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;

    // the specify their count at pipeline creation time
    // withoutr dynamic state they need to be set in the pipeline and make the
    // pipeline immutable, any changes require a new pipeline
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // Rastorizer takes the geometry that is shaped by the vertices from the
    // vertex shader & turns it into fragments to be colored by fragment shader
    // it also performs deep testing, face culling, scissor test & can be
    // figured to output fragments that fill entire polygons or justthe edges
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable =
        VK_FALSE; /// if enabled fragments that are beyond the near & far planes
                  /// are clamped to them (useful for shadowmaps & enables a GPU
                  /// feature)
    rasterizer.rasterizerDiscardEnable =
        VK_FALSE; /// if true geomtry never passes through the rasterizer stage,
                  /// basically disables output to the framebuffer

    rasterizer.polygonMode =
        VK_POLYGON_MODE_FILL; /// determines how  fragments are generated for
                              /// geometry, other than fill requires GPU-feature
    rasterizer.lineWidth =
        1.0f; /// thickness of lines in terms of fragments, thicker lines
              /// requires the "wideLines-GPU-feature"
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT; /// type of faceculling to use
    rasterizer.frontFace =
        VK_FRONT_FACE_CLOCKWISE; /// specifies the vertex order for faces to be
                                 /// considered
    rasterizer.depthBiasEnable =
        VK_FALSE; /// used for shadow mapping, wont use it for now
    rasterizer.depthBiasConstantFactor = 0.0f; /// optional
    rasterizer.depthBiasClamp = 0.0f;          /// optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    /// optional

    // Multisampling is one way to perform anti-aliasing
    // works by combining fragment shader results of multiple polygons that
    // rasterize the same pixel mainly occurs along edges, its less expensive
    // than rendering to a higher resolution & then downscaling. enabling
    // requires a GPU-feature, disabled for now
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;          // Optional
    multisampling.pSampleMask = nullptr;            // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    // if using depth/stencil buffer one need to fill the
    // VkPipelineDepthStencilStateCreateInfo struct, for now nullptr

    // after a fragment has returned a color it needs to be combined with the
    // color that is already in the framebuffer, this is known as ColorBlending
    // two structs are needed to be filled
    // the transformation can be done in 2 ways:
    // a) Mix the old and new value to produce a final color
    // b) Combine the old and new value using a bitwise operation
    // we also disable both modes for now
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable =
        VK_FALSE; // new color from the fragment shader is passed through
                  // unmodified
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // one can use uniform values in shaders, (= globals similar to dynamic
    // state variables that can be changed at drawing time to alter the behavior
    // of the shaders without having to recreate them. commonly used to pass the
    // transformation matrix to the vertex shader, or to create texture samplers
    // in the fragment shader. These uniform values need to be specified during
    // pipeline creation by creating a VkPipelineLayout object.
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;         // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr;      // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges =
        nullptr; // Optional, another way of passing dynamic values to shaders

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                               &pipelineLayout) != VK_SUCCESS) {
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
    // Vulkan allows you to create a new graphics pipeline from an existing one
    // (less expensove) at the moment we only have a single pipeline, so we
    // specify a nullHandle and invalid index only used if
    // VK_PIPELINE_CREATE_DERIVATIVE_BIT is also specified
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1;              // Optional

    // final step !
    // VkPipelineCache can be used to store & reuse data relevant to pipeline
    // creation across multiple calls to vkCreateGraphicsPipeline
    //
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &graphicsPipeline) != VK_SUCCESS) {
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
   * the image which is used depends on which image the swap chain returns when
   * we retrieve one for representation
   * We have to create a framebuffer for all the images in the swap chain and
   * use the one that corresponds to the retrieved image at drawing time.
   */
  void createFramebuffers() {
    // size it to the hold imageViews
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
      VkImageView attachments[] = {swapChainImageViews[i]};
      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      // specifiy which renderpass the framebuffer needs to be compatible with
      // they need to use the same number and types of attachments
      framebufferInfo.renderPass = renderPass;
      // specify the VkImageView objects
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = swapChainExtent.width;
      framebufferInfo.height = swapChainExtent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                              &swapChainFramebuffers[i]) != VK_SUCCESS) {
        throw std::runtime_error("failed tp create framebuffer");
      }
    }
  }

  /**
   * Command pools are opaque objects that command buffer memory is allocated
   * from, and which allow the implementation to amortize the cost of resource
   * creation across multiple command buffers.
   * */
  void createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags =
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; /// allow command
                                                         /// buffers to be
                                                         /// rerecorded
                                                         /// individually,
                                                         /// without they have
                                                         /// to be reset
                                                         /// together
    // command buffers are executed by submitting them on one of the device
    // queues each command can only allocate command buffers that are submitted
    // on a single type of queue we chose commands for drawing so graphics
    // queue family is chosen
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create command pool!");
    }
  }

  void createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level =
        VK_COMMAND_BUFFER_LEVEL_PRIMARY; /// Can be submitted to a queue for
                                         /// execution, but cannot be called
                                         /// from other command buffers.
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to allocate command buffers!");
    }
  }

  void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags =
        VK_FENCE_CREATE_SIGNALED_BIT; /// first call to vkWaitForFence() returns
                                      /// immediately instead of waiting for
                                      /// previous frames (which do not exist at
                                      /// the beginning)

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr,
                          &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) !=
            VK_SUCCESS) {
      throw std::runtime_error("failed to create semaphores!");
    }
  }

  /**
   * function that writes the commands we want to execute into a command buffer.
   **/
  void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; /// dont use atm                  /// optional
    beginInfo.pInheritanceInfo =
        nullptr; /// optional only for secondary command buffers

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    // size of the renderArea, defines where shader loads and stors will take
    // place, pixel outside this area will have undefined values
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1; /// clear values
    renderPassInfo.pClearValues =
        &clearColor; /// for VK_ATTACHMENT_LOAD_OP_CLEAR

    vkCmdBeginRenderPass(
        commandBuffer, &renderPassInfo,
        VK_SUBPASS_CONTENTS_INLINE); /// render pass commands will be embedded
                                     /// in the primary command buffer

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline);

    // as viewport and scissor state is dynamic we need to set them in cmmand
    // buffer before drawing

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

    // vertexCount = 3, instanceCount = 1 (for instanced rendering),
    // firstVertex: used as an offest into the vertex buffer defines the lowest
    // value of gl_VertexIndex firstInstance:Used as an offset for instanced
    // rendering, defines the lowest value of gl_InstanceIndex
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer");
    }
  }

  /*
  ** Helper function to wrap a shaderBuffer to a VkShaderModule object
  */
  VkShaderModule createShaderModule(const std::vector<char> &code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) !=
        VK_SUCCESS) {
      throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
  }

  void populateDebugMessengerCreateInfo(
      VkDebugUtilsMessengerCreateInfoEXT &createInfo) {

    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = /// sepcify all types of severity to let tha
                                 /// callback be called for
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo
        .messageType = /// which types of messages the callback is notified for
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback =
        debugCallback; /// pointer to the callback function
  }

  void setupDebugMessenger() {
    if (not enableValidationLayers)
      return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    // requires a valid instance have been created -> to validate all code
    // before this we can use a sepatare debug utils messenger
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr,
                                     &debugMesseger) != VK_SUCCESS) {
      throw std::runtime_error("failed to set up debug messenger!");
    }
  }

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  // logical device to interface with
  // could setup more logical device from one physical device for different
  // requirements
  VkDevice device;
  // queues are automatically created with the logical device but we need a
  // handle to interface with, the are implicitly cleaned up with the device
  VkQueue graphicsQueue;
  VkSurfaceKHR surface;
  VkQueue presentQueue;
  VkSwapchainKHR swapChain;
  std::vector<VkImage> swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  // to use any VkImage, we need a view into a image, it described how to access
  // the image and which part of the image, should be treated as 2D depth
  // texture wothout any mimapping levels
  std::vector<VkImageView> swapChainImageViews;
  VkPipelineLayout pipelineLayout;
  VkRenderPass renderPass;
  VkPipeline graphicsPipeline;
  std::vector<VkFramebuffer> swapChainFramebuffers; /// holds all framebuffers

  VkCommandPool
      commandPool; /// manages the memory that is used to store the buffers and
                   /// command buffers which are allocated from them
  VkCommandBuffer commandBuffer;
  VkDebugUtilsMessengerEXT debugMesseger;
  GLFWwindow *window;
  VkInstance instance;
  // synchronization
  VkSemaphore imageAvailableSemaphore;
  VkSemaphore renderFinishedSemaphore;
  VkFence inFlightFence;
};

int main() {

  TriangleApp app;
  try {
    app.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
