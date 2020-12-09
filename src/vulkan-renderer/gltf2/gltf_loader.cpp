#include "inexor/vulkan-renderer/gltf2/gltf_loader.hpp"
#include <glm/gtc/type_ptr.hpp>

namespace inexor::vulkan_renderer::gltf2 {

Model::Model(const wrapper::Device &device, std::string &file_name, std::string &model_name)
    : m_device(device), m_name(model_name), m_file_name(file_name) {
    tinygltf::TinyGLTF gltf_context;

    std::string gltf_loader_error = "";
    std::string gltf_loader_warning = "";

    if (!gltf_context.LoadASCIIFromFile(&m_gltf_model, &gltf_loader_error, &gltf_loader_warning, file_name)) {
        throw std::runtime_error("Could not load glTF2 file " + file_name + "!");
    }

    load_textures();
    load_materials();
    load_texture_indices();

    // TODO: Support more than 1 scene!
    const tinygltf::Scene &scene = m_gltf_model.scenes[0];

    for (std::size_t i = 0; i < scene.nodes.size(); i++) {
        load_node(m_gltf_model.nodes[scene.nodes[i]], m_gltf_model, nullptr, m_index_data, m_vertex_data);
    }

    // TODO: Implement!
    setup_descriptor_sets();

    // TODO: Create mesh buffer!
}

Model::~Model() {}

void Model::load_textures() {
    m_textures.reserve(m_gltf_model.images.size());

    for (auto &gltf_image : m_gltf_model.images) {
        // We convert RGB-only images to RGBA, as most devices don't support RGB-formats in Vulkan
        if (gltf_image.component == 3) {
            std::size_t pixel_count =
                static_cast<std::size_t>(gltf_image.width) * static_cast<std::size_t>(gltf_image.height);
            std::vector<unsigned char> texture_data(pixel_count * 4);

            unsigned char *rgba = texture_data.data();
            unsigned char *rgb = gltf_image.image.data();

            // Convert RGB values to RGBA values
            for (std::size_t i = 0; i < pixel_count; i++) {
                std::memcpy(rgba, rgb, sizeof(unsigned char) * 3);
                rgba += 4;
                rgb += 3;
            }

            // TODO: Is it correct to use 4 for channels RGBA?
            m_textures.emplace_back(m_device, texture_data.data(), texture_data.size(), gltf_image.width,
                                    gltf_image.height, 4, 1, std::string("gltf image"));

        } else if (gltf_image.component == 4) {
            // TODO: Is it correct to use 4 for channels RGBA?
            m_textures.emplace_back(m_device, gltf_image.image.data(), gltf_image.image.size(), gltf_image.width,
                                    gltf_image.height, 4, 1, std::string("gltf image"));
        } else {
            throw std::runtime_error("Unknown number of channels in gltf image!");
        }
    }
}

void Model::load_texture_indices() {
    m_texture_indices.resize(m_gltf_model.textures.size());

    for (size_t i = 0; i < m_gltf_model.textures.size(); i++) {
        m_texture_indices[i] = m_gltf_model.textures[i].source;
    }
}

void Model::load_materials() {
    m_materials.resize(m_gltf_model.materials.size());

    for (size_t i = 0; i < m_gltf_model.materials.size(); i++) {
        // We only read the most basic properties required for our sample
        tinygltf::Material glTFMaterial = m_gltf_model.materials[i];

        // Get the base color factor.
        if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
            m_materials[i].base_color_factor =
                glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
        }
        // Get base color texture index.
        if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
            m_materials[i].base_color_texture_index = glTFMaterial.values["baseColorTexture"].TextureIndex();
        }
    }
}

void Model::load_node(const tinygltf::Node &inputNode, const tinygltf::Model &input, std::shared_ptr<ModelNode> parent,
                      std::vector<uint32_t> &indexBuffer, std::vector<ModelVertex> &vertexBuffer) {

    std::shared_ptr<ModelNode> node = std::make_shared<ModelNode>();
    node->matrix = glm::mat4(1.0f);

    // Get the local node matrix
    // It's either made up from translation, rotation, scale or a 4x4 matrix
    if (inputNode.translation.size() == 3) {
        node->matrix = glm::translate(node->matrix, glm::vec3(glm::make_vec3(inputNode.translation.data())));
    }
    if (inputNode.rotation.size() == 4) {
        glm::quat q = glm::make_quat(inputNode.rotation.data());
        node->matrix *= glm::mat4(q);
    }
    if (inputNode.scale.size() == 3) {
        node->matrix = glm::scale(node->matrix, glm::vec3(glm::make_vec3(inputNode.scale.data())));
    }
    if (inputNode.matrix.size() == 16) {
        node->matrix = glm::make_mat4x4(inputNode.matrix.data());
    };

    // Load node's children
    if (inputNode.children.size() > 0) {
        for (size_t i = 0; i < inputNode.children.size(); i++) {
            load_node(input.nodes[inputNode.children[i]], input, node, indexBuffer, vertexBuffer);
        }
    }

    // If the node contains mesh data, we load vertices and indices from the buffers
    // In glTF this is done via accessors and buffer views
    if (inputNode.mesh > -1) {
        const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
        // Iterate through all primitives of this node's mesh
        for (size_t i = 0; i < mesh.primitives.size(); i++) {
            const tinygltf::Primitive &glTFPrimitive = mesh.primitives[i];
            uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
            uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
            uint32_t indexCount = 0;
            // Vertices
            {
                const float *positionBuffer = nullptr;
                const float *normalsBuffer = nullptr;
                const float *texCoordsBuffer = nullptr;
                size_t vertexCount = 0;

                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor &accessor =
                        input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                    const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                    positionBuffer = reinterpret_cast<const float *>(
                        &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    vertexCount = accessor.count;
                }
                // Get buffer data for vertex normals
                if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor &accessor =
                        input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                    const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                    normalsBuffer = reinterpret_cast<const float *>(
                        &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }
                // Get buffer data for vertex texture coordinates
                // glTF supports multiple sets, we only load the first one
                if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end()) {
                    const tinygltf::Accessor &accessor =
                        input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                    const tinygltf::BufferView &view = input.bufferViews[accessor.bufferView];
                    texCoordsBuffer = reinterpret_cast<const float *>(
                        &(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                }

                // Append data to model's vertex buffer
                for (size_t v = 0; v < vertexCount; v++) {
                    ModelVertex vert{};
                    vert.position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                    vert.normal = glm::normalize(
                        glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                    vert.uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                    vert.color = glm::vec3(1.0f);
                    vertexBuffer.push_back(vert);
                }
            }
            // Indices
            {
                const tinygltf::Accessor &accessor = input.accessors[glTFPrimitive.indices];
                const tinygltf::BufferView &bufferView = input.bufferViews[accessor.bufferView];
                const tinygltf::Buffer &buffer = input.buffers[bufferView.buffer];

                indexCount += static_cast<uint32_t>(accessor.count);

                // glTF supports different component types of indices.
                switch (accessor.componentType) {
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                    std::vector<std::uint32_t> added_indices(accessor.count);

                    std::memcpy(added_indices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                                accessor.count * sizeof(uint32_t));

                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(added_indices[index] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                    std::vector<std::uint16_t> added_indices(accessor.count);

                    std::memcpy(added_indices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                                accessor.count * sizeof(uint16_t));

                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(added_indices[index] + vertexStart);
                    }
                    break;
                }
                case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                    std::vector<std::uint8_t> added_indices(accessor.count);

                    std::memcpy(added_indices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset],
                                accessor.count * sizeof(uint8_t));

                    for (size_t index = 0; index < accessor.count; index++) {
                        indexBuffer.push_back(added_indices[index] + vertexStart);
                    }
                    break;
                }
                default:
                    spdlog::error("Index component type %d not supported!", accessor.componentType);
                    return;
                }
            }
            ModelPrimitive primitive{};

            primitive.first_index = firstIndex;
            primitive.index_count = indexCount;
            primitive.material_index = glTFPrimitive.material;

            node->mesh.primitives.push_back(primitive);
        }
    }

    if (parent) {
        parent->children.push_back(node);
    } else {
        m_model_nodes.push_back(node);
    }
}

void Model::draw_node(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, std::shared_ptr<ModelNode> node) {
    if (node->mesh.primitives.size() > 0) {
        // Pass the node's matrix via push constants
        // Traverse the node hierarchy to the top-most parent to get the final matrix of the current node
        glm::mat4 nodeMatrix = node->matrix;
        auto current_parent = node->parent;

        while (current_parent) {
            nodeMatrix = current_parent->matrix * nodeMatrix;
            current_parent = current_parent->parent;
        }

        // Pass the final matrix to the vertex shader using push constants
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
                           &nodeMatrix);

        for (ModelPrimitive &primitive : node->mesh.primitives) {
            if (primitive.index_count > 0) {

                // Get the texture index for this primitive.
                std::int32_t texture =
                    m_texture_indices[m_materials[primitive.material_index].base_color_texture_index];

                // TODO! Setup descriptor sets!
                // Bind the descriptor for the current primitive's texture.
                // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1,
                //                        &m_text[texture.imageIndex].descriptorSet, 0, nullptr);

                // TODO: Fix this!

                vkCmdDrawIndexed(commandBuffer, primitive.index_count, 1, primitive.first_index, 0, 0);
            }
        }
    }
    for (auto &child : node->children) {
        draw_node(commandBuffer, pipelineLayout, child);
    }
}

void Model::draw(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout) {
    // All vertices and indices are stored in single buffers, so we only need to bind once
    VkDeviceSize offsets[1] = {0};

    const auto &vertex_buffer = m_model_mesh->get_vertex_buffer();
    const auto &index_buffer = m_model_mesh->get_index_buffer();

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertex_buffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

    // Render all nodes at top-level
    for (auto &node : m_model_nodes) {
        draw_node(commandBuffer, pipelineLayout, node);
    }
}

}; // namespace inexor::vulkan_renderer::gltf2
