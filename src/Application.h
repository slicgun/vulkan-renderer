#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "Renderer/Vulkan/Image.h"
#include "Renderer/Vulkan/Buffer.h"
#include "Renderer/Vulkan/Texture.h"

struct Vertex;
struct UniformBufferObject;

class Application
{
public:
    Application();
    ~Application();
    void update();

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
private:
    void drawFrame();
    void updateUniformBuffer(uint32_t currentImage);
    void recreateSwapChain();

    void initVulkan();
    void initGlfw();
    void cleanup();
    void cleanupSwapChain();

    void createInstance();
    void setupDebugMessenger();
    void pickPhysicalDevice();
    void createLogicalDevice();

    void createSurface();
    void createSwapChain();
    void createImageViews();
    void createFramebuffers();

    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();

    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
    void createSyncObjects();

    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();

    void createTextureImage();
    void createDepthResources();

    //shader
    vk::ShaderModule createShaderModule(const std::vector<char>& code);

    GLFWwindow* m_window;
    const uint32_t m_windowHeight = 600;
    const uint32_t m_windowWidth  = 800;

    vk::Instance m_instance;
    vk::PhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    vk::Device m_device;

    vk::Queue m_graphicsQueue;
    vk::Queue m_presentQueue;

    vk::SurfaceKHR m_surface;
    vk::SwapchainKHR m_swapChain;
    std::vector<vk::Image> m_swapChainImages;
    std::vector<vk::ImageView> m_swapChainImageViews;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;

    std::vector<vk::Framebuffer> m_swapChainFramebuffers;

    vk::RenderPass m_renderPass;

    vk::DescriptorSetLayout m_descriptorSetLayout;
    vk::DescriptorPool m_descriptorPool;
    vk::PipelineLayout m_pipelineLayout;
    std::vector<vk::DescriptorSet> m_descriptorSets;

    vk::Pipeline m_graphicsPipeline;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;
    bool m_framebufferResized = false;
    uint32_t m_currentFrame = 0;

    Renderer::Vulkan::Buffer m_vertexBuffer;
    Renderer::Vulkan::Buffer m_indexBuffer;
    std::vector<Renderer::Vulkan::Buffer> m_uniformBuffers;

    Renderer::Vulkan::Texture m_texture;
    Renderer::Vulkan::Image m_depthImage;

    VkDebugUtilsMessengerEXT m_debugMessenger;

    const bool m_enableValidationLayers = true; //this should be part of a macro but i prefer to do it like this when im just messing around with stuff
    const std::vector<const char*> m_validationLayers =
    {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> m_deviceExtensions =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    const uint32_t m_maxFramesInFlight = 3;
};