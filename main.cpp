#include <bits/stdint-uintn.h>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

const uint32_t WINDOW_WIDTH = 800;
const uint32_t WINDOW_HEIGHT = 600;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation", "Test1"};

// clang-format off
#ifdef NDEBUG
  const bool enableValidationLayers = false;
#else
  const bool enableValidationLayers = true;
#endif
// clang-format on

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

  void initVulkan() { createInstance(); }

  void mainLoop() {
    while (not glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  void cleanup() {
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

    // GLFW has built-in function that returns the extensionscount
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtension;
    glfwExtension = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtension;
    // global validation layer determination, for now we dont use
    createInfo.enabledLayerCount = 0;

    // could check for extension support (see instance page bottom)

    // 3. finally create the instance and check result
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      throw std::runtime_error("failed to create instance");
    }
  }

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
