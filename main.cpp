#include <GLFW/glfw3native.h>
#include <bits/stdint-uintn.h>
#include <cstddef>
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
    createGraphicsPipeline();
  }

  void mainLoop() {
    while (not glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
    // must be destryoed before the instance -> to validate all code after this
    // we can use a sepatare debug utils messenger
    if (enableValidationLayers) {
      DestroyDebugUtilsMessengerEXT(instance, debugMesseger, nullptr);
    }

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
      // make sure that the evctor can hold all formats
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
  */
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

  // clang-format off
  /*
  ** The graphics pipeline is the sequence of operations that take the vertices
  *  & textures of the meshes all the way to render the pixels in the render
  *  targets.
  ** Vertex/index buffer -> Input Assembler -> Vertex shader ->Tessellation -> Geometry shader -> Rasterization -> Fragment shader -> Color blending -> Frame buffer
  */
  // clang-format on
  void createGraphicsPipeline() {
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    // compilation & linking from SPIR-V bytecode to machinecode will not happen
    // until the graphic pipeline is created so we need local variables ...
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
    vertShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    vertShaderStageInfo.module = fragShaderModule;
    // entrypoint - function
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertShaderStageInfo,
        fragShaderStageInfo}; //.. and need to cleanup them here ...

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
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

  VkDebugUtilsMessengerEXT debugMesseger;
  GLFWwindow *window;
  VkInstance instance;
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
