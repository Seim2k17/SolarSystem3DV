#include <bits/stdint-uintn.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

const uint32_t WINDOW_WIDTH = 800;
const uint32_t WINDOW_HEIGHT = 600;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

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

struct QueueFamilyIndices {
  // graphicsFamily could have a value or not
  std::optional<uint32_t> graphicsFamily;

  bool isComplete() { return graphicsFamily.has_value(); }
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
    pickPhysicalDevice();
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

    // destroy the instance right before the window
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

    auto extensions = getRequiredExtesions();
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
  std::vector<const char *> getRequiredExtesions() {
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
      if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = 1;
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
    return indices.isComplete();
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
