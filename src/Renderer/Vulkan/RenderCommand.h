#pragma once

#include <vulkan/vulkan.hpp>

//this is to be changed
//bad code
//and unsafe cos unchecked pointers
namespace Renderer::Vulkan
{
	class RenderCommand
	{
	public:
		RenderCommand() = default;

		static void initRenderCommands(vk::Device& device, vk::CommandPool& commandPool, vk::Queue& graphicsQueue);

		static vk::CommandBuffer beginSingleTimeCommands();
		static void endSingleTimeCommands(vk::CommandBuffer commandBuffer);

	private:
		static vk::Device* m_sDevice;
		static vk::CommandPool* m_sCommandPool;
		static vk::Queue* m_sGraphicsQueue;
	};
}