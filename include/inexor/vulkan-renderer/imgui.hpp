#pragma once

#include "inexor/vulkan-renderer/frame_graph.hpp"
#include "inexor/vulkan-renderer/wrapper/command_buffer.hpp"
#include "inexor/vulkan-renderer/wrapper/command_pool.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptor.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"
#include "inexor/vulkan-renderer/wrapper/fence.hpp"
#include "inexor/vulkan-renderer/wrapper/framebuffer.hpp"
#include "inexor/vulkan-renderer/wrapper/gpu_texture.hpp"
#include "inexor/vulkan-renderer/wrapper/graphics_pipeline.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/mesh_buffer.hpp"
#include "inexor/vulkan-renderer/wrapper/renderpass.hpp"
#include "inexor/vulkan-renderer/wrapper/shader.hpp"
#include "inexor/vulkan-renderer/wrapper/swapchain.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace inexor::vulkan_renderer {

class ImGUIOverlay {
    const wrapper::Device &m_device;
    const wrapper::Swapchain &m_swapchain;

    float m_scale{1.0f};

    std::unique_ptr<wrapper::GpuTexture> m_imgui_texture;
    std::unique_ptr<wrapper::Shader> m_vert_shader;
    std::unique_ptr<wrapper::Shader> m_frag_shader;
    std::unique_ptr<wrapper::CommandPool> m_command_pool;
    std::unique_ptr<wrapper::ResourceDescriptor> m_descriptor;

    std::uint32_t m_vertex_count{0};
    std::uint32_t m_index_count{0};

    std::vector<VkPipelineShaderStageCreateInfo> m_shaders;
    std::vector<std::unique_ptr<wrapper::CommandBuffer>> m_command_buffers;
    std::vector<std::unique_ptr<wrapper::Framebuffer>> m_framebuffers;

    BufferResource *m_index_buffer{nullptr};
    BufferResource *m_vertex_buffer{nullptr};

    // TODO: Implement an RAII wrapper for push constants!
    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } m_push_const_block{};

    bool should_update();
    void update_buffers(FrameGraph *frame_graph);
    void update(const PhysicalStage *phys, const wrapper::CommandBuffer &cmd_buf);

public:
    /// @brief Default constructor
    /// @param device A reference to the device wrapper.
    /// @param swapchain A reference to the swapchain.
    ImGUIOverlay(const wrapper::Device &device, const wrapper::Swapchain &swapchain, FrameGraph *frame_graph,
                 TextureResource *back_buffer);
    ImGUIOverlay(const ImGUIOverlay &) = delete;
    ImGUIOverlay(ImGUIOverlay &&) = delete;
    ~ImGUIOverlay();

    ImGUIOverlay &operator=(const ImGUIOverlay &) = delete;
    ImGUIOverlay &operator=(ImGUIOverlay &&) = delete;

    [[nodiscard]] float scale() const {
        return m_scale;
    }
};

} // namespace inexor::vulkan_renderer
