#pragma once
#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <vector>

const uint32_t WINDOW_WIDTH = 1280;
const uint32_t WINDOW_HEIGHT = 1024;

class SolEngine {
  public:
    SolEngine(){};

    static SolEngine &Get();
    void cleanup();
    // draw loop
    void draw();
    void init();
    void initWindow();
    // run main loop
    void run();

    /* Vulkan handles section */
    VkInstance _instance;                      // Vulkan library handle
    VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
    VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
    VkDevice _device;            // Vulkan device for commands
    VkSurfaceKHR _surface;       // Vulkan window surface
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    VkExtent2D _swapchainExtend;
    /* Vulkan handles section end */

    bool _isInitialized{false};
    bool framebufferResized{false};
    int _frameNumber{0};
    bool stop_rendering{false};
    VkExtent2D _windowExtend{WINDOW_WIDTH, WINDOW_HEIGHT};
    struct GLFWwindow *window{nullptr};

  private:
    void create_swapchain(uint32_t width, uint32_t height);
    void destroy_swapchain();
    void init_commands();
    void init_swapchain();
    void init_sync_structures();
    void init_vulkan();
    void setupWindowSurface();
};
