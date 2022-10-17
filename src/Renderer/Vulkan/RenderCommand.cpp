#include "RenderCommand.h"

vk::Device* Renderer::Vulkan::RenderCommand::m_sDevice = nullptr;
vk::CommandPool* Renderer::Vulkan::RenderCommand::m_sCommandPool = nullptr;
vk::Queue* Renderer::Vulkan::RenderCommand::m_sGraphicsQueue = nullptr;

void Renderer::Vulkan::RenderCommand::initRenderCommands(vk::Device& device, vk::CommandPool& commandPool, vk::Queue& graphicsQueue)
{
    m_sDevice = &device;
    m_sCommandPool = &commandPool;
    m_sGraphicsQueue = &graphicsQueue;
}

vk::CommandBuffer Renderer::Vulkan::RenderCommand::beginSingleTimeCommands()
{
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandPool(*m_sCommandPool);
    allocInfo.setCommandBufferCount(1);

    vk::CommandBuffer commandBuffer = m_sDevice->allocateCommandBuffers(allocInfo)[0];

    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Renderer::Vulkan::RenderCommand::endSingleTimeCommands(vk::CommandBuffer commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBufferCount(1);
    submitInfo.setPCommandBuffers(&commandBuffer);

    m_sGraphicsQueue->submit(submitInfo);
    m_sGraphicsQueue->waitIdle();

    m_sDevice->freeCommandBuffers(*m_sCommandPool, 1, &commandBuffer);
}
