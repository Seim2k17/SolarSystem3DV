#pragma once
#include <vulkan/vulkan_core.h>

#include <cstdint>

const uint32_t WINDOW_WIDTH = 1280;
const uint32_t WINDOW_HEIGHT = 1024;

class SolEngine {
  public:
    SolEngine(){};

    static SolEngine &Get();
    void init();
    void initWindow();
    void cleanup();
    // draw loop
    void draw();
    // run main loop
    void run();

    /* Vulkan handles section */
    VkInstance _instance;                      // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device;            // Vulkan device for commands
    VkSurfaceKHR _surface;       // Vulkan window surface
    /* Vulkan handles section end */

    bool _isInitialized{false};
    bool framebufferResized{false};
    int _frameNumber{0};
    bool stop_rendering{false};
    VkExtent2D _windowExtend{WINDOW_WIDTH, WINDOW_HEIGHT};
    struct GLFWwindow *window{nullptr};

  private:
    void init_vulkan();
    void init_swapchain();
    void init_commands();
    void init_sync_structures();
};
