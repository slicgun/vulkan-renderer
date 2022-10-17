#include "VulkanUtils.h"

#include <set>
#include <string>
#include <stdexcept>

namespace VulkanUtils
{

    std::vector<const char*> getRequiredExtensions(bool validationLayersEnabled)
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if(validationLayersEnabled)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    //this body can have stuff in the future so will make call to this even tho it seems pointless
    bool isDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface, const std::vector<const char*> deviceExtensions)
    {
        QueueFamilyIndices indices = findQueueFamilies(device, surface);
        bool extensionsSupported = checkDeviceExtensionSupport(device, deviceExtensions);
        bool hasSwapChain = false;

        if(extensionsSupported)
        {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
            hasSwapChain = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();

        return indices.graphicsFamily.has_value() && extensionsSupported && hasSwapChain && supportedFeatures.samplerAnisotropy;
    }

    bool checkDeviceExtensionSupport(vk::PhysicalDevice device, const std::vector<const char*> deviceExtensions)
    {
        auto availableExtensions = device.enumerateDeviceExtensionProperties();
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for(const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    {
        int i = 0;
        QueueFamilyIndices indices;

        auto queueFamilies = device.getQueueFamilyProperties();

        for(const auto& queueFamily : queueFamilies)
        {
            if(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
                indices.graphicsFamily = i;

            if(device.getSurfaceSupportKHR(i, surface))
                indices.presentFamily = i;

            if(indices.isComplete())
                break;

            i++;
        }

        return indices;
    }

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    {
        SwapChainSupportDetails details;
        device.getSurfaceCapabilitiesKHR(surface, &details.capabilities);
        details.formats = device.getSurfaceFormatsKHR(surface);
        details.presentModes = device.getSurfacePresentModesKHR(surface);

        return details;
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties, vk::PhysicalDevice device)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = device.getMemoryProperties();

        for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if(typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    vk::Format findSupportedFormat(vk::PhysicalDevice device, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features)
    {
        for(vk::Format format : candidates)
        {
            vk::FormatProperties props = device.getFormatProperties(format);

            if(tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
                return format;
            else if(tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
                return format;
        }

        throw std::runtime_error("failed to find supported format!");
    }

    vk::Format findDepthFormat(vk::PhysicalDevice device)
    {
        return findSupportedFormat(device,
                                   {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                                   vk::ImageTiling::eOptimal,
                                   vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    bool hasStencilComponent(vk::Format format)
    {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    vk::ImageView createImageView(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags)
    {
        vk::ImageSubresourceRange imgSubresourceRange;
        imgSubresourceRange.setAspectMask(aspectFlags);
        imgSubresourceRange.setBaseMipLevel(0);
        imgSubresourceRange.setLevelCount(1);
        imgSubresourceRange.setBaseArrayLayer(0);
        imgSubresourceRange.setLayerCount(1);

        vk::ImageViewCreateInfo viewInfo;
        viewInfo.setImage(image);
        viewInfo.setViewType(vk::ImageViewType::e2D);
        viewInfo.setFormat(format);
        viewInfo.setSubresourceRange(imgSubresourceRange);

        return device.createImageView(viewInfo);
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
    {
        for(const auto& availableFormat : availableFormats)
        {
            if(availableFormat.format == vk::Format::eB8G8R8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return availableFormat;
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
    {
        for(const auto& availablePresentMode : availablePresentModes)
        {
            if(availablePresentMode == vk::PresentModeKHR::eMailbox)
                return availablePresentMode;
        }
        return vk::PresentModeKHR::eFifo; //better for low energy usage
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, const glm::ivec2& framebufferSize)
    {
        if(capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actualExtent =
            {
                static_cast<uint32_t>(framebufferSize.x),
                static_cast<uint32_t>(framebufferSize.y)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    bool QueueFamilyIndices::isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
}
