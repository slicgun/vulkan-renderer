#pragma once

#include<vector>
#include<optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include<glm/vec2.hpp>

#include<vulkan/vulkan.hpp>

namespace VulkanUtils
{
	struct QueueFamilyIndices 
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		bool isComplete();
	};

	struct SwapChainSupportDetails
	{
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> presentModes;
	};

	std::vector<const char*> getRequiredExtensions(bool validationLayersEnabled);

	bool isDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*> deviceExtensions);
	bool checkDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*> deviceExtensions);

	QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface);
	SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface);

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, vk::PhysicalDevice device);

	vk::Format findSupportedFormat(vk::PhysicalDevice device, const std::vector<VkFormat>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);
	vk::Format findDepthFormat(vk::PhysicalDevice device);
	bool hasStencilComponent(vk::Format format);

	vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags);

	//swap chain
	vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats);
	vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes);
	vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const glm::ivec2& framebufferSize);
}