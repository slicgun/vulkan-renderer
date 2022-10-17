#pragma once

#include <vulkan.hpp>

#include "Image.h"

namespace Renderer::Vulkan
{
	class SwapChain
	{
	public:
		SwapChain(vk::Device& device, vk::PhysicalDevice& physicalDevice);

		void create();
		void createFramebuffers();
	private:
		vk::SurfaceKHR m_surface;
		vk::SwapchainKHR m_swapChain;
		vk::Format m_imageFormat;
		vk::Extent2D m_Extent;

		std::vector<Image> m_images;
		std::vector<vk::Framebuffer> m_framebuffers;

		vk::Device& m_device;
		vk::PhysicalDevice& m_physicalDevice;
	};
}