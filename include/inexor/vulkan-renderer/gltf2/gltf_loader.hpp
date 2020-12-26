#pragma once

#include "inexor/vulkan-renderer/tools/file.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"
#include "inexor/vulkan-renderer/wrapper/gpu_texture.hpp"
#include "inexor/vulkan-renderer/wrapper/mesh_buffer.hpp"

#include <glm/glm.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <memory>
#include <string>
#include <vector>

namespace inexor::vulkan_renderer::wrapper {

class Device;

};

namespace inexor::vulkan_renderer::gltf2 {

/// @brief A wrapper for loading glTF2 models.
class Model {

    const wrapper::Device &m_device;
    std::string m_name;
    std::string m_file_name;
    tinygltf::Model m_gltf_model;

    /// @brief  TODO: Name consistently!!!! !

    /// @brief Since we might load textures directly from the glTF file, we need a GPU texture buffer.
    std::vector<wrapper::GpuTexture> m_textures{};

    /// @brief
    struct Material {
        glm::vec4 base_color_factor = glm::vec4(1.0f);
        std::uint32_t base_color_texture_index{0};
    };

    /// @brief
    struct ModelVertex {
        glm::vec3 position{0.0f, 0.0f, 0.0f};
        glm::vec3 color{0.0f, 0.0f, 0.0f};
        glm::vec3 normal{0.0f, 0.0f, 0.0f};
        glm::vec2 uv{0.0f, 0.0f};
    };

    std::unique_ptr<wrapper::MeshBuffer<ModelVertex>> m_model_mesh{};

    std::vector<std::int32_t> m_texture_indices;
    std::vector<Material> m_materials;

    struct ModelPrimitive {
        std::uint32_t first_index;
        std::uint32_t index_count;
        std::int32_t material_index;
    };

    struct ModelMesh {
        std::vector<ModelPrimitive> primitives{};
    };

    // A node represents an object in the glTF scene graph
    struct ModelNode {
        // TODO: Discuss if we really need unique ptr!
        std::shared_ptr<ModelNode> parent;
        std::vector<std::shared_ptr<ModelNode>> children;
        ModelMesh mesh;
        glm::mat4 matrix;
    };

    std::vector<std::shared_ptr<ModelNode>> m_model_nodes;
    std::vector<uint32_t> m_index_data;
    std::vector<ModelVertex> m_vertex_data;

    void load_textures();

    void load_texture_indices();

    void load_materials();

    /// @brief
    /// @param inputNode
    /// @param input
    /// @param parent
    /// @param indexBuffer
    /// @param vertexBuffer
    void load_node(const tinygltf::Node &inputNode, const tinygltf::Model &input, std::shared_ptr<ModelNode> parent,
                   std::vector<uint32_t> &indexBuffer, std::vector<ModelVertex> &vertexBuffer);

    /// @brief
    void setup_descriptor_sets();

public:
    /// @brief
    /// @param device
    /// @param file_name
    /// @param model_name
    Model(const wrapper::Device &device, std::string &file_name, std::string &model_name);

    ~Model();

    /// @brief Draw a single node including child nodes (if present).
    /// @param commandBuffer
    /// @param pipelineLayout
    /// @param node
    void draw_node(VkCommandBuffer cmd_buffer, VkPipelineLayout layout, std::shared_ptr<ModelNode> node);

    /// @brief Draw the glTF scene starting at the top-level-nodes.
    /// @param commandBuffer
    /// @param pipelineLayout
    void draw(VkCommandBuffer cmd_buffer, VkPipelineLayout layout);
};

}; // namespace inexor::vulkan_renderer::gltf2
