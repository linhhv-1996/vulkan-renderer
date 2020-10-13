#pragma once

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan.h>

#include <cassert>

namespace inexor::vulkan_renderer::wrapper {

/// @brief RAII wrapper class for VkSurfaceKHR.
class WindowSurface {
    VkInstance m_instance;
    VkSurfaceKHR m_surface;

public:
    /// @brief Default constructor.
    /// @param instance The Vulkan instance which will be associated with this surface.
    /// @param window The window which will be associated with this surface.
    WindowSurface(VkInstance instance, GLFWwindow *window);

    WindowSurface(const WindowSurface &) = delete;
    WindowSurface(WindowSurface &&) noexcept;

    ~WindowSurface();

    WindowSurface &operator=(const WindowSurface &) = delete;
    WindowSurface &operator=(WindowSurface &&) = default;

    [[nodiscard]] VkSurfaceKHR get() const {
        return m_surface;
    }
};

} // namespace inexor::vulkan_renderer::wrapper
