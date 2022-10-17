#include "Image.h"

#include "utils/VulkanUtils.h"

#include "RenderCommand.h"

Renderer::Vulkan::Image::Image(vk::Device& device, vk::PhysicalDevice& physicalDevice)
    :m_device(device), m_physicalDevice(physicalDevice)
{
}

void Renderer::Vulkan::Image::create(vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlags aspectFlags)
{
    //create image
    vk::Extent3D imgExtent;
    imgExtent.setWidth(m_width);
    imgExtent.setHeight(m_height);
    imgExtent.setDepth(1);

    vk::ImageCreateInfo imageInfo;
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setExtent(imgExtent);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setFormat(format);
    imageInfo.setTiling(tiling);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setUsage(usage);
    imageInfo.setSamples(vk::SampleCountFlagBits::e1);
    imageInfo.setSharingMode(vk::SharingMode::eExclusive);

    m_image = m_device.createImage(imageInfo);

    vk::MemoryRequirements memRequirements = m_device.getImageMemoryRequirements(m_image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.setAllocationSize(memRequirements.size);
    allocInfo.setMemoryTypeIndex(VulkanUtils::findMemoryType(memRequirements.memoryTypeBits, properties, m_physicalDevice));

    m_imageMemory = m_device.allocateMemory(allocInfo);
    m_device.bindImageMemory(m_image, m_imageMemory, 0);

    createImageView(format, aspectFlags);
}

void Renderer::Vulkan::Image::createImageView(vk::Format format, vk::ImageAspectFlags aspectFlags)
{
    //create image view
    vk::ImageSubresourceRange imgSubresourceRange;
    imgSubresourceRange.setAspectMask(aspectFlags);
    imgSubresourceRange.setBaseMipLevel(0);
    imgSubresourceRange.setLevelCount(1);
    imgSubresourceRange.setBaseArrayLayer(0);
    imgSubresourceRange.setLayerCount(1);

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.setImage(m_image);
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setFormat(format);
    viewInfo.setSubresourceRange(imgSubresourceRange);

    m_imageView = m_device.createImageView(viewInfo);
}

void Renderer::Vulkan::Image::transitionImageLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    vk::CommandBuffer commandBuffer = RenderCommand::beginSingleTimeCommands();

    vk::ImageSubresourceRange barrierSubResourceRange;
    barrierSubResourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    barrierSubResourceRange.setBaseMipLevel(0);
    barrierSubResourceRange.setLevelCount(1);
    barrierSubResourceRange.setBaseArrayLayer(0);
    barrierSubResourceRange.setLayerCount(1);

    std::array<vk::ImageMemoryBarrier, 1> barriers;
    barriers[0].setOldLayout(oldLayout);
    barriers[0].setNewLayout(newLayout);
    barriers[0].setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barriers[0].setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barriers[0].setImage(m_image);
    barriers[0].setSubresourceRange(barrierSubResourceRange);

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if(newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barriers[0].subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if(VulkanUtils::hasStencilComponent(format))
            barriers[0].subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
    else
    {
        barriers[0].subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }


    if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
    {
        barriers[0].srcAccessMask = vk::AccessFlagBits::eNone;
        barriers[0].dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if(oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
        barriers[0].srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barriers[0].dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if(oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
    {
        barriers[0].srcAccessMask = vk::AccessFlagBits::eNone;
        barriers[0].dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }
    else
        throw std::invalid_argument("unsupported layout transition!");

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, static_cast<vk::DependencyFlagBits>(0), {}, {}, barriers);

    RenderCommand::endSingleTimeCommands(commandBuffer);
}

void Renderer::Vulkan::Image::copyFromBuffer(const Buffer& buffer, vk::ImageAspectFlagBits aspectFlag)
{
    vk::CommandBuffer commandBuffer = RenderCommand::beginSingleTimeCommands();

    std::array<vk::BufferImageCopy, 1> regions;
    regions[0].setBufferOffset(0);
    regions[0].setBufferRowLength(0);
    regions[0].setBufferImageHeight(0);

    vk::ImageSubresourceLayers imgSubResoruceLayers;
    imgSubResoruceLayers.setAspectMask(aspectFlag);
    imgSubResoruceLayers.setMipLevel(0);
    imgSubResoruceLayers.setBaseArrayLayer(0);
    imgSubResoruceLayers.setLayerCount(1);

    regions[0].setImageSubresource(imgSubResoruceLayers);
    regions[0].setImageOffset({0, 0, 0});
    regions[0].setImageExtent({m_width, m_height, 1});

    commandBuffer.copyBufferToImage(buffer.getHandle(), m_image, vk::ImageLayout::eTransferDstOptimal, regions);

    RenderCommand::endSingleTimeCommands(commandBuffer);
}

void Renderer::Vulkan::Image::free()
{
    m_device.destroyImageView(m_imageView);
    m_device.destroyImage(m_image);
    m_device.freeMemory(m_imageMemory);
    m_width = m_height = 0;
}
