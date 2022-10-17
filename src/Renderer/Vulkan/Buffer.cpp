#include "Buffer.h"

#include "utils/VulkanUtils.h"
#include "RenderCommand.h"
#include "Vertex.h"

Renderer::Vulkan::Buffer::Buffer(vk::Device& device, vk::PhysicalDevice& physicalDevice)
	:m_device(&device), m_physicalDevice(&physicalDevice)
{
}

void Renderer::Vulkan::Buffer::create(uint32_t bufferSize, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
    m_size = bufferSize;

    //create buffer
    vk::BufferCreateInfo bufferInfo;
    bufferInfo.setSize(m_size);
    bufferInfo.setUsage(usage);
    bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

    m_buffer = m_device->createBuffer(bufferInfo);

    //reserve (gpu) memory
    vk::MemoryRequirements memRequirements = m_device->getBufferMemoryRequirements(m_buffer);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.setAllocationSize(memRequirements.size);
    allocInfo.setMemoryTypeIndex(VulkanUtils::findMemoryType(memRequirements.memoryTypeBits, properties, *m_physicalDevice));

    m_memory = m_device->allocateMemory(allocInfo);

    //bind memory and buffer
    m_device->bindBufferMemory(m_buffer, m_memory, 0);
}

void Renderer::Vulkan::Buffer::copyBuffer(const Buffer& other)
{
    vk::CommandBuffer commandBuffer = Renderer::Vulkan::RenderCommand::beginSingleTimeCommands();

    vk::BufferCopy copyRegion;
    copyRegion.setSize(other.m_size);
    commandBuffer.copyBuffer(other.m_buffer, m_buffer, copyRegion);

    Renderer::Vulkan::RenderCommand::endSingleTimeCommands(commandBuffer);
}

void Renderer::Vulkan::Buffer::free()
{
    if(m_device == nullptr || m_physicalDevice == nullptr || m_size == 0)
    {
        m_size = 0;
        m_device = nullptr;
        m_physicalDevice = nullptr;
        return;
    }

    m_device->destroyBuffer(m_buffer);
    m_device->freeMemory(m_memory);
    m_device = nullptr;
    m_physicalDevice = nullptr;
    m_size = 0;
}
