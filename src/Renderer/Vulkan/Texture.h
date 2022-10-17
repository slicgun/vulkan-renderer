#pragma once

#include <vulkan/vulkan.hpp>
#include <string>

#include "Image.h"

#include<stb_image.h>

namespace Renderer::Vulkan
{
	class Texture
	{
	public:
		Texture(vk::Device& device, vk::PhysicalDevice& physicalDevice, const std::string& filename = "");

		void create(const std::string& filename, vk::Filter filter, vk::SamplerAddressMode addressMode);

		vk::Image getHandle() { return m_image.getHandle(); }
		vk::ImageView getImageView() { return m_image.getImageView(); }
		vk::Sampler getSampler() { return m_sampler; }
	private:
		bool loadTexture(const std::string& filename);
	private:
		vk::Device& m_device;
		vk::PhysicalDevice& m_physicalDevice;
		Image m_image;
		vk::Sampler m_sampler;

		stbi_uc* m_pixels = nullptr;
		int m_width = 0;
		int m_height = 0;
		int m_channels = 0;
	};
}