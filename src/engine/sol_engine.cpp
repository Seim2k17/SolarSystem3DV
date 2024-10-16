#include "sol_engine.h"

#include <GLFW/glfw3.h>

#include <cassert>
#include <chrono>
#include <memory>
#include <thread>

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

    // everything went fine
    _isInitialized = true;
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
