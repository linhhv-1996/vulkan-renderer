#include "inexor/vulkan-renderer/wrapper/descriptor_builder.hpp"

#include "inexor/vulkan-renderer/wrapper/descriptor.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"

#include <spdlog/spdlog.h>

namespace inexor::vulkan_renderer::wrapper {

DescriptorBuilder::DescriptorBuilder(const Device &device, std::uint32_t swapchain_image_count)
    : m_device(device), m_swapchain_image_count(swapchain_image_count) {
    assert(m_device.device());
    assert(m_swapchain_image_count > 0);
}

ResourceDescriptor DescriptorBuilder::build(std::string name) {
    assert(!m_layout_bindings.empty());
    assert(!m_write_sets.empty());
    assert(m_write_sets.size() == m_layout_bindings.size());

    // Generate a new resource descriptor.
    ResourceDescriptor generated_descriptor(m_device, m_swapchain_image_count, std::move(m_layout_bindings),
                                            std::move(m_write_sets), std::move(name));

    m_descriptor_buffer_infos.clear();
    m_descriptor_image_infos.clear();

    return std::move(generated_descriptor);
}

DescriptorBuilder &DescriptorBuilder::add_combined_image_sampler(const VkSampler image_sampler,
                                                                 const VkImageView image_view,
                                                                 const std::uint32_t binding,
                                                                 const VkShaderStageFlagBits shader_stage) {

    // Call other class method to avoid code duplication.
    return add_combined_image_sampler_array({image_sampler}, {image_view}, {binding}, {shader_stage});
}

DescriptorBuilder &DescriptorBuilder::add_combined_image_sampler_array(
    const std::vector<VkSampler> image_samplers, const std::vector<VkImageView> image_views,
    const std::vector<std::uint32_t> bindings, const std::vector<VkShaderStageFlagBits> shader_stages) {

    assert(!image_samplers.empty());
    assert(!image_views.empty());
    assert(!bindings.empty());
    assert(!shader_stages.empty());
    assert(image_samplers.size() == image_views.size() == bindings.size() == shader_stages.size());

    // Let's not use a range-based for loop here.
    // Use a normal loop and check sampler, view, binding and shader stage for each index.
    // Do not assert binding indices, as they might be zero which would cause the assertion to fail.
    for (std::size_t i = 0; i < image_samplers.size(); i++) {
        assert(image_samplers[i]);
        assert(image_views[i]);
        assert(shader_stages[i]);
    }

    // Only if every index is valid, we create the entries.
    // Let's also use not a range-based for loop here.
    for (std::size_t i = 0; i < image_samplers.size(); i++) {

        VkDescriptorSetLayoutBinding layout_binding{};
        layout_binding.binding = 0;
        layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        layout_binding.descriptorCount = 1;
        layout_binding.stageFlags = shader_stages[i];

        m_layout_bindings.push_back(layout_binding);

        VkDescriptorImageInfo image_info{};
        image_info.sampler = image_samplers[i];
        image_info.imageView = image_views[i];
        image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        m_descriptor_image_infos.push_back(image_info);

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = nullptr;
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pImageInfo = &m_descriptor_image_infos.back();

        m_write_sets.push_back(descriptor_write);
    }

    return *this;
}

} // namespace inexor::vulkan_renderer::wrapper
