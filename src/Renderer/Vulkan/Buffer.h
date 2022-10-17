#pragma once

#include <vulkan/vulkan.hpp>

#include <stb_image.h>

namespace Renderer::Vulkan
{
	class Buffer
	{
	public:
		Buffer() = default;
		Buffer(vk::Device& device, vk::PhysicalDevice& physicalDevice);

		void create(uint32_t bufferSize, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
		void copyBuffer(const Buffer& other);
		void free();

		vk::Buffer getHandle() const { return m_buffer; }

		void setDevices(vk::Device& device, vk::PhysicalDevice& physicalDevice) { m_device = &device; m_physicalDevice = &physicalDevice; }

		template<typename T>
		void allocateAndMap(const std::vector<T>& data)
		{
			//allocate memory and transfer to gpu
			void* allocated = m_device->mapMemory(m_memory, 0, m_size);
			memcpy(allocated, data.data(), static_cast<size_t>(m_size));
			m_device->unmapMemory(m_memory);
		}
	private:
		vk::DeviceSize m_size = 0;

		vk::Buffer m_buffer;
		vk::DeviceMemory m_memory;

		vk::Device* m_device = nullptr;
		vk::PhysicalDevice* m_physicalDevice = nullptr;
	};
}

//buffer has staging buffer + actual buffer it will use
//use same logic as before
