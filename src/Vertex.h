#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
	glm::vec3 position = glm::vec3(0.f);
	glm::vec3 color = glm::vec3(0.f);
	glm::vec2 texCoord = glm::vec2(0.f);

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		vk::VertexInputBindingDescription bindingDescription;
		bindingDescription.setBinding(0);
		bindingDescription.setStride(sizeof(Vertex));
		bindingDescription.setInputRate(vk::VertexInputRate::eVertex);
		return bindingDescription;
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};
		attributeDescriptions[0].setBinding(0);
		attributeDescriptions[0].setLocation(0);
		attributeDescriptions[0].setFormat(vk::Format::eR32G32B32Sfloat);
		attributeDescriptions[0].setOffset(offsetof(Vertex, position));

		attributeDescriptions[1].setBinding(0);
		attributeDescriptions[1].setLocation(1);
		attributeDescriptions[1].setFormat(vk::Format::eR32G32B32Sfloat);
		attributeDescriptions[1].setOffset(offsetof(Vertex, color));

		attributeDescriptions[2].setBinding(0);
		attributeDescriptions[2].setLocation(2);
		attributeDescriptions[2].setFormat(vk::Format::eR32G32Sfloat);
		attributeDescriptions[2].setOffset(offsetof(Vertex, texCoord));

		return attributeDescriptions;
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};