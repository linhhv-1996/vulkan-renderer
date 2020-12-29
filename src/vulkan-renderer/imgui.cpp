#include "inexor/vulkan-renderer/imgui.hpp"

#include "inexor/vulkan-renderer/exceptions/vk_exception.hpp"
#include "inexor/vulkan-renderer/frame_graph.hpp"
#include "inexor/vulkan-renderer/wrapper/cpu_texture.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptor_builder.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"

#include <cassert>
#include <stdexcept>

namespace inexor::vulkan_renderer {

ImGUIOverlay::ImGUIOverlay(const wrapper::Device &device, const wrapper::Swapchain &swapchain, FrameGraph *frame_graph,
                           TextureResource *back_buffer)
    : m_device(device), m_swapchain(swapchain) {
    assert(device.device());
    assert(device.physical_device());
    assert(device.allocator());
    assert(device.graphics_queue());

    spdlog::debug("Creating ImGUI context");
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    ImGuiStyle &style = ImGui::GetStyle();
    io.FontGlobalScale = m_scale;
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);

    spdlog::debug("Loading ImGUI shaders");
    // TODO: Don't really need to be members or unique_ptrs.
    m_vert_shader = std::make_unique<wrapper::Shader>(m_device, VK_SHADER_STAGE_VERTEX_BIT, "ImGUI vertex shader",
                                                      "shaders/ui.vert.spv");
    m_frag_shader = std::make_unique<wrapper::Shader>(m_device, VK_SHADER_STAGE_FRAGMENT_BIT, "ImGUI fragment shader",
                                                      "shaders/ui.frag.spv");

    // Initialize push constant block.
    // TODO: not needed.
    m_push_const_block.scale = glm::vec2(0.0f, 0.0f);
    m_push_const_block.translate = glm::vec2(0.0f, 0.0f);

    // Load font texture.
    // TODO: Move this data into a container class; have container class also support bold and italic.
    constexpr const char *FONT_FILE_PATH = "assets/fonts/NotoSans-Bold.ttf";
    constexpr float FONT_SIZE = 18.0f;
    spdlog::debug("Loading front '{}'", FONT_FILE_PATH);
    ImFont *font = io.Fonts->AddFontFromFileTTF(FONT_FILE_PATH, FONT_SIZE);

    unsigned char *font_texture_data = nullptr;
    int font_texture_width = 0;
    int font_texture_height = 0;
    io.Fonts->GetTexDataAsRGBA32(&font_texture_data, &font_texture_width, &font_texture_height);
    if (font == nullptr || font_texture_data == nullptr) {
        spdlog::error("Unable to load font {}.  Falling back to error texture.", FONT_FILE_PATH);
        m_imgui_texture = std::make_unique<wrapper::GpuTexture>(m_device, wrapper::CpuTexture());
    } else {
        // Our font textures always have 4 channels and a single mip level by definition.
        constexpr int FONT_TEXTURE_CHANNELS = 4;
        constexpr int FONT_MIP_LEVELS = 1;
        spdlog::debug("Creating ImGUI font texture");
        VkDeviceSize upload_size = static_cast<VkDeviceSize>(font_texture_width) *
                                   static_cast<VkDeviceSize>(font_texture_height) *
                                   static_cast<VkDeviceSize>(FONT_TEXTURE_CHANNELS);
        m_imgui_texture = std::make_unique<wrapper::GpuTexture>(
            m_device, font_texture_data, upload_size, font_texture_width, font_texture_height, FONT_TEXTURE_CHANNELS,
            FONT_MIP_LEVELS, "ImGUI font texture");
    }

    m_command_pool = std::make_unique<wrapper::CommandPool>(m_device, m_device.graphics_queue_family_index());

    // Create an instance of the resource descriptor builder.
    // This allows us to make resource descriptors with the help of a builder pattern.
    wrapper::DescriptorBuilder descriptor_builder(m_device, m_swapchain.image_count());

    // Make use of the builder to create a resource descriptor for the combined image sampler.
    m_descriptor = std::make_unique<wrapper::ResourceDescriptor>(
        descriptor_builder.add_combined_image_sampler(m_imgui_texture->sampler(), m_imgui_texture->image_view(), 0)
            .build("ImGUI"));

    VkPushConstantRange push_constant_range{};
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PushConstBlock);
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    m_index_buffer = &frame_graph->add<BufferResource>("imgui index buffer");
    m_index_buffer->set_usage(BufferUsage::INDEX_BUFFER);

    m_vertex_buffer = &frame_graph->add<BufferResource>("imgui vertex buffer");
    m_vertex_buffer->set_usage(BufferUsage::VERTEX_BUFFER);
    m_vertex_buffer->add_vertex_attribute(VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, pos));
    m_vertex_buffer->add_vertex_attribute(VK_FORMAT_R32G32_SFLOAT, offsetof(ImDrawVert, uv));
    m_vertex_buffer->add_vertex_attribute(VK_FORMAT_R8G8B8A8_UNORM, offsetof(ImDrawVert, col));

    auto &stage = frame_graph->add<GraphicsStage>("imgui stage");
    stage.writes_to(*back_buffer);
    stage.reads_from(*m_index_buffer);
    stage.reads_from(*m_vertex_buffer);
    stage.add_descriptor_layout(m_descriptor->descriptor_set_layout());
    stage.add_push_constant_range(push_constant_range);
    stage.bind_buffer(*m_vertex_buffer, 0);
    stage.uses_shader(*m_vert_shader);
    stage.uses_shader(*m_frag_shader);
    stage.set_dynamic(true);
    stage.set_should_record([this] { return should_update(); });
    stage.set_pre_record([this](FrameGraph *frame_graph) { update_buffers(frame_graph); });
    stage.set_on_record(
        [this](const PhysicalStage *phys, const wrapper::CommandBuffer &cmd_buf) { update(phys, cmd_buf); });
}

ImGUIOverlay::~ImGUIOverlay() {
    spdlog::trace("Destroying ImGUI context");
    ImGui::DestroyContext();
}

static bool g_a = true;

bool ImGUIOverlay::should_update() {
    if (g_a) {
        g_a = false;
        return true;
    }

    ImDrawData *imgui_draw_data = ImGui::GetDrawData();
    if (imgui_draw_data == nullptr) {
        return false;
    }

    const VkDeviceSize vertex_buffer_size = imgui_draw_data->TotalVtxCount * sizeof(ImDrawVert);
    const VkDeviceSize index_buffer_size = imgui_draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (vertex_buffer_size == 0 || index_buffer_size == 0) {
        return false;
    }

    bool update_command_buffers = false;
    if (m_vertex_count != imgui_draw_data->TotalVtxCount) {
        m_vertex_count = imgui_draw_data->TotalVtxCount;
        update_command_buffers = true;
    }

    if (m_index_count < static_cast<std::uint32_t>(imgui_draw_data->TotalIdxCount)) {
        m_index_count = imgui_draw_data->TotalIdxCount;
        update_command_buffers = true;
    }
    return update_command_buffers;
}

void ImGUIOverlay::update_buffers(FrameGraph *frame_graph) {
    auto *draw_data = ImGui::GetDrawData();
    auto *vertex_buffer_address = static_cast<ImDrawVert *>(
        frame_graph->upload_to_buffer(m_vertex_buffer, draw_data->TotalVtxCount * sizeof(ImDrawVert), nullptr));
    auto *index_buffer_address = static_cast<ImDrawIdx *>(
        frame_graph->upload_to_buffer(m_index_buffer, draw_data->TotalIdxCount * sizeof(ImDrawIdx), nullptr));
    for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
        const ImDrawList *cmd_list = draw_data->CmdLists[i];
        std::memcpy(vertex_buffer_address, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        std::memcpy(index_buffer_address, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vertex_buffer_address += cmd_list->VtxBuffer.Size;
        index_buffer_address += cmd_list->IdxBuffer.Size;
    }
}

void ImGUIOverlay::update(const PhysicalStage *phys, const wrapper::CommandBuffer &cmd_buf) {
    auto *draw_data = ImGui::GetDrawData();
    const auto &io = ImGui::GetIO();
    m_push_const_block.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    m_push_const_block.translate = glm::vec2(-1.0f);
    cmd_buf.bind_descriptor(*m_descriptor, phys->pipeline_layout());
    cmd_buf.push_constants(VK_SHADER_STAGE_VERTEX_BIT, sizeof(PushConstBlock), &m_push_const_block,
                           phys->pipeline_layout());
    std::int32_t vertex_offset = 0;
    std::int32_t index_offset = 0;
    for (std::int32_t i = 0; i < draw_data->CmdListsCount; i++) {
        const ImDrawList *cmd_list = draw_data->CmdLists[i];
        for (std::int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
            // TODO: Use CommandBuffer::draw_indexed() here, we can't right now since we are using dynamic offsets.
            const ImDrawCmd *draw_cmd = &cmd_list->CmdBuffer[j];
            vkCmdDrawIndexed(cmd_buf.get(), draw_cmd->ElemCount, 1, index_offset, vertex_offset, 0);
            index_offset += draw_cmd->ElemCount;
        }
        vertex_offset += cmd_list->VtxBuffer.Size;
    }
}

} // namespace inexor::vulkan_renderer
