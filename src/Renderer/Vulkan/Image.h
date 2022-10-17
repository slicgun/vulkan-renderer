#pragma once

#include <vulkan/vulkan.hpp>

#include "RenderCommand.h"
#include "Buffer.h"

namespace Renderer::Vulkan
{
	class Image
	{
	public:
		Image(vk::Device& device, vk::PhysicalDevice& physicalDevice);

		void setDevices(vk::Device& device, vk::PhysicalDevice& physicalDevice) { m_device = device; m_physicalDevice = physicalDevice; }
		void setSize(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

		void create(vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::ImageAspectFlags aspectFlags);
		void transitionImageLayout(vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

		void copyFromBuffer(const Buffer& buffer, vk::ImageAspectFlagBits aspectFlag);

		void free();

		vk::Image getHandle() { return m_image; }
		vk::ImageView getImageView() { return m_imageView; };
		void createImageView(vk::Format format, vk::ImageAspectFlags aspectFlags);
	private:
		uint32_t m_width, m_height = 0;

		vk::Image m_image;
		vk::DeviceMemory m_imageMemory;
		vk::ImageView m_imageView;

		vk::Device& m_device;
		vk::PhysicalDevice& m_physicalDevice;
	};

}