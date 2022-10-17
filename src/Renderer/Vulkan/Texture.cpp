#include "Texture.h"

#include <iostream>

#include "Buffer.h"

Renderer::Vulkan::Texture::Texture(vk::Device& device, vk::PhysicalDevice& physicalDevice, const std::string& filename)
	:m_device(device), m_physicalDevice(physicalDevice), m_image(device, physicalDevice)
{
}

bool Renderer::Vulkan::Texture::loadTexture(const std::string& filename)
{
    m_pixels = stbi_load("res/textures/test.png", &m_width, &m_height, &m_channels, STBI_rgb_alpha);

    if(!m_pixels)
    {
        std::cout<<"failed to load texture image!\n";
        return false;
    }
    return true;
}

void Renderer::Vulkan::Texture::create(const std::string& filename, vk::Filter filter, vk::SamplerAddressMode addressMode)
{
    if(!loadTexture(filename))
    {
        std::cout << "failed to load file\n";
        return;
    }

    //create buffer to store pixels
    uint32_t imgSize = m_channels * m_width * m_height;
    Buffer stagingBuffer(m_device, m_physicalDevice);
    stagingBuffer.create(imgSize, 
                         vk::BufferUsageFlagBits::eTransferSrc, 
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    //do this because if i pass a std::vector<stbi_uc*>
    //a stbi_uc** will be uploaded to gpu and this causes the texture data to be a random mess
    //due to the fact that it is reading a pointer value rather than pixel data
    std::vector<stbi_uc> pixels(m_pixels, m_pixels + imgSize);
    stagingBuffer.allocateAndMap<stbi_uc>(pixels);

    //create image
    m_image.setSize(m_width, m_height);
    m_image.create(vk::Format::eR8G8B8A8Srgb,
                   vk::ImageTiling::eOptimal,
                   vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                   vk::MemoryPropertyFlagBits::eDeviceLocal,
                   vk::ImageAspectFlagBits::eColor);

    //transition layouts and copy buffer data to image
    m_image.transitionImageLayout(vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    m_image.copyFromBuffer(stagingBuffer, vk::ImageAspectFlagBits::eColor);
    m_image.transitionImageLayout(vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::PhysicalDeviceProperties properties = m_physicalDevice.getProperties();

    //create sampler
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.setMagFilter(filter);
    samplerInfo.setMinFilter(filter);
    samplerInfo.setAddressModeU(addressMode);
    samplerInfo.setAddressModeV(addressMode);
    samplerInfo.setAddressModeW(addressMode);
    samplerInfo.setAnisotropyEnable(true);
    samplerInfo.setMaxAnisotropy(properties.limits.maxSamplerAnisotropy);
    samplerInfo.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    samplerInfo.setUnnormalizedCoordinates(false);
    samplerInfo.setCompareEnable(false);
    samplerInfo.setCompareOp(vk::CompareOp::eAlways);
    samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    samplerInfo.setMipLodBias(0.0f);
    samplerInfo.setMinLod(0.0f);
    samplerInfo.setMaxLod(0.0f);

    m_sampler = m_device.createSampler(samplerInfo);

    stbi_image_free(m_pixels);
    stagingBuffer.free();
}
