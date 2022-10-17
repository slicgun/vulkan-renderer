#include "Application.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <vector>
#include <set>
#include <limits>
#include <algorithm>
#include <chrono>

#include "utils/DebugUtils.h"
#include "utils/VulkanUtils.h"
#include "utils/Utils.h"

#include "Renderer/Vulkan/RenderCommand.h"

#include "Vertex.h"

#include "Renderer/Vulkan/Texture.h"

#include <stb_image.h>


using VulkanUtils::SwapChainSupportDetails;
using VulkanUtils::QueueFamilyIndices;

const std::vector<Vertex> vertices =
{
    {{-0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 1.0f}, {1.f, 0.f}},
    {{ 0.5f, -0.5f, 0.f}, {1.0f, 0.0f, 1.0f}, {0.f, 0.f}},
    {{ 0.5f,  0.5f, 0.f}, {0.0f, 0.0f, 1.0f}, {0.f, 1.f}},
    {{-0.5f,  0.5f, 0.f}, {1.0f, 0.0f, 0.0f}, {1.f, 1.f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {1.f, 0.f}},
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 1.0f}, {0.f, 0.f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.f, 1.f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.f, 1.f}},
};

const std::vector<uint32_t> indices =
{
    0, 1, 2,
    2, 3, 0,

    4, 5, 6,
    6, 7, 4,
};

Application::Application()
    :m_depthImage(m_device, m_physicalDevice)
    , m_vertexBuffer(m_device, m_physicalDevice)
    , m_indexBuffer(m_device, m_physicalDevice)
    , m_texture(m_device, m_physicalDevice)
{
    initGlfw();
    initVulkan();
}

Application::~Application()
{
    cleanup();
}

void Application::update()
{
    double start = 0;
    double end = 0;
    while(!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        drawFrame();
        end = glfwGetTime();
        //std::cout << (end - start) * 1000.f << "ms\n";
        start = end;
    }
    m_device.waitIdle();
}

void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
    app->m_framebufferResized = true;
}

void Application::drawFrame()
{
    //wait for previous frame to finish
    m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    //get image from swap chain
    
    auto nextImgKHR = m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame]);
    uint32_t imageIndex = nextImgKHR.value;

    if(m_framebufferResized || nextImgKHR.result == vk::Result::eErrorOutOfDateKHR)
    {
        m_framebufferResized = false;
        recreateSwapChain();
        return;
    }
    else if(nextImgKHR.result != vk::Result::eSuccess && nextImgKHR.result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("failed to aquire swap chain image!");
    }

    //reset fence if work is being submitted to avoid deadlock
    m_device.resetFences(1, &m_inFlightFences[m_currentFrame]);

    //record command buffer
    m_commandBuffers[m_currentFrame].reset();
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

    updateUniformBuffer(m_currentFrame);

    //submit command buffer
    vk::SubmitInfo submitInfo;

    vk::Semaphore waitSemaphores[]  = {m_imageAvailableSemaphores[m_currentFrame]};
    vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
    submitInfo.setWaitSemaphoreCount(1);
    submitInfo.setPWaitSemaphores(waitSemaphores);
    submitInfo.setPWaitDstStageMask(waitStages);

    submitInfo.setCommandBufferCount(1);
    submitInfo.setPCommandBuffers(&m_commandBuffers[m_currentFrame]);

    vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.setSignalSemaphoreCount(1);
    submitInfo.setPSignalSemaphores(signalSemaphores);

    m_graphicsQueue.submit(submitInfo, m_inFlightFences[m_currentFrame]);

    vk::SwapchainKHR swapChains[] = {m_swapChain};
    vk::PresentInfoKHR presentInfo;
    presentInfo.setWaitSemaphoreCount(1);
    presentInfo.setPWaitSemaphores(signalSemaphores);

    presentInfo.setSwapchainCount(1);
    presentInfo.setPSwapchains(swapChains);
    presentInfo.setPImageIndices(&imageIndex);
    presentInfo.setPResults(nullptr);

    vk::Result presentKHRResult = m_presentQueue.presentKHR(presentInfo);

    if(presentKHRResult == vk::Result::eErrorOutOfDateKHR || presentKHRResult == vk::Result::eSuboptimalKHR || m_framebufferResized)
    {
        m_framebufferResized = false;
        recreateSwapChain();
    }
    else if(presentKHRResult != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }


    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;
}

void Application::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo;
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1; //flip image because glm was designed for opengl

    m_uniformBuffers[currentImage].allocateAndMap<UniformBufferObject>({ubo});
}

void Application::recreateSwapChain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);

    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    m_device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
    createFramebuffers();
}

void Application::initVulkan()
{
    //order is important
    createInstance();
    setupDebugMessenger();

    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();

    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();

    createCommandPool();

    createDepthResources();

    createFramebuffers();

    createTextureImage();

    createVertexBuffer();
    createIndexBuffer();

    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();

    createCommandBuffers();
    createSyncObjects();

    DebugUtils::printExtensionsInfo();
}

void Application::initGlfw()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "Vulkan window", nullptr, nullptr);
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

void Application::cleanup()
{
    if(m_enableValidationLayers) 
        DebugUtils::DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
}

void Application::cleanupSwapChain()
{
    m_depthImage.free();

    for(auto framebuffer : m_swapChainFramebuffers)
        m_device.destroyFramebuffer(framebuffer);

    for(auto imageView : m_swapChainImageViews)
        m_device.destroyImageView(imageView);

    m_device.destroySwapchainKHR(m_swapChain);
}

void Application::createInstance()
{
    //create app info
    vk::ApplicationInfo appInfo;
    appInfo.setPApplicationName("hello triangle");
    appInfo.setApplicationVersion(VK_MAKE_VERSION(1, 0, 0));
    appInfo.setPEngineName("no engine");
    appInfo.setEngineVersion(VK_MAKE_VERSION(1, 0, 0));
    appInfo.setApiVersion(VK_API_VERSION_1_0);

    //check validation layers support
    if(m_enableValidationLayers && !DebugUtils::checkValidationLayerSupport(m_validationLayers))
        throw std::runtime_error("validation layers requested, but not available!");

    //create vulkan instance
    vk::InstanceCreateInfo createInfo;
    createInfo.setPApplicationInfo(&appInfo);

    //add extensions
    auto extensions = VulkanUtils::getRequiredExtensions(m_enableValidationLayers);
    createInfo.setEnabledExtensionCount(static_cast<uint32_t>(extensions.size()));
    createInfo.setPEnabledExtensionNames(extensions);

    //add validation layer(s)
    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo;
    if(m_enableValidationLayers)
    {
        createInfo.setEnabledLayerCount(static_cast<uint32_t>(m_validationLayers.size()));
        createInfo.setPEnabledLayerNames(m_validationLayers);

        DebugUtils::populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.setPNext(&debugCreateInfo);
    }
    else
    {
        createInfo.setEnabledLayerCount(0);
        createInfo.setPNext(nullptr);
    }

    m_instance = vk::createInstance(createInfo);
}

void Application::setupDebugMessenger()
{
    if(!m_enableValidationLayers)
        return;
    
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    DebugUtils::populateDebugMessengerCreateInfo(createInfo);
    DebugUtils::CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
}

void Application::pickPhysicalDevice()
{
    auto devices = m_instance.enumeratePhysicalDevices();

    if(devices.empty())
        throw std::runtime_error("failed to find GPUs with Vulkan support!");

    for(const auto& device : devices)
    {
        if(VulkanUtils::isDeviceSuitable(device, m_surface, m_deviceExtensions))
        {
            m_physicalDevice = device;
            return;
        }
    }

    std::cout << "failed to find GPUs with Vulkan support!";
}

void Application::createLogicalDevice()
{
    float queuePriority = 1.f;
    QueueFamilyIndices indices = VulkanUtils::findQueueFamilies(m_physicalDevice, m_surface);
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    //create q infos
    for(uint32_t queueFamily : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.setQueueFamilyIndex(queueFamily);
        queueCreateInfo.setQueueCount(1);
        queueCreateInfo.setPQueuePriorities(&queuePriority);
        queueCreateInfos.push_back(queueCreateInfo);
    }

    vk::DeviceCreateInfo createInfo;
    vk::PhysicalDeviceFeatures deviceFeatures;
    deviceFeatures.samplerAnisotropy = true;

    //create devcie info
    createInfo.setQueueCreateInfoCount(static_cast<uint32_t>(queueCreateInfos.size()));
    createInfo.setPQueueCreateInfos(queueCreateInfos.data());
    createInfo.setPEnabledFeatures(&deviceFeatures);

    createInfo.setEnabledExtensionCount(static_cast<uint32_t>(m_deviceExtensions.size()));
    createInfo.setPEnabledExtensionNames(m_deviceExtensions);

    //add validation layer info (backward compatability, not needed for new versions of vulkan)
    if(m_enableValidationLayers)
    {
        createInfo.setEnabledLayerCount(static_cast<uint32_t>(m_validationLayers.size()));
        createInfo.setPEnabledLayerNames(m_validationLayers);
    }
    else
    {
        createInfo.setEnabledLayerCount(0);
    }

     m_device = m_physicalDevice.createDevice(createInfo);

    //set up graphics queue, the 0 is the queue count/index
    m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
    m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
}

void Application::createSurface()
{
    VkSurfaceKHR surface;
    if(glfwCreateWindowSurface(m_instance, m_window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
    m_surface = surface;
}

void Application::createSwapChain()
{
    glm::ivec2 framebufferSize;
    glfwGetFramebufferSize(m_window, &framebufferSize.x, &framebufferSize.y);

    //get swap chain details
    SwapChainSupportDetails swapChainSupport = VulkanUtils::querySwapChainSupport(m_physicalDevice, m_surface);

    vk::SurfaceFormatKHR surfaceFormat = VulkanUtils::chooseSwapSurfaceFormat(swapChainSupport.formats);
    vk::PresentModeKHR presentMode = VulkanUtils::chooseSwapPresentMode(swapChainSupport.presentModes);
    vk::Extent2D extent = VulkanUtils::chooseSwapExtent(swapChainSupport.capabilities, framebufferSize);

    //set img count
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        imageCount = swapChainSupport.capabilities.maxImageCount;

    //create swap chain info
    vk::SwapchainCreateInfoKHR createInfo{};
    createInfo.setSurface(m_surface);
    createInfo.setMinImageCount(imageCount);
    createInfo.setImageFormat(surfaceFormat.format);
    createInfo.setImageColorSpace(surfaceFormat.colorSpace);
    createInfo.setImageExtent(extent);
    createInfo.setImageArrayLayers(1);
    createInfo.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    //set queue family info
    QueueFamilyIndices indices = VulkanUtils::findQueueFamilies(m_physicalDevice, m_surface);
    std::array<uint32_t, 2> queueFamilyIndices = {indices.graphicsFamily.value(), indices.presentFamily.value()};

    //swap chain usage across multiple families
    if(indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
        createInfo.setQueueFamilyIndexCount(queueFamilyIndices.size());
        createInfo.setPQueueFamilyIndices(queueFamilyIndices.data());
    }
    else
    {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        createInfo.setQueueFamilyIndexCount(0);
        createInfo.setPQueueFamilyIndices(nullptr);
    }

    //render settings
    createInfo.setPreTransform(swapChainSupport.capabilities.currentTransform);
    createInfo.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    createInfo.setPresentMode(presentMode);
    createInfo.setClipped(true);
    createInfo.setOldSwapchain(VK_NULL_HANDLE);

    m_swapChain = m_device.createSwapchainKHR(createInfo);

    //store info for retrieving swap chain images
    m_swapChainImages = m_device.getSwapchainImagesKHR(m_swapChain);
    m_swapChainImageFormat = surfaceFormat.format;
    m_swapChainExtent = extent;
}

void Application::createImageViews()
{
    m_swapChainImageViews.resize(m_swapChainImages.size());

    //create all swap chain image views
    for(size_t i = 0; i < m_swapChainImages.size(); i++)
    {
        m_swapChainImageViews[i] = VulkanUtils::createImageView(m_device, m_swapChainImages[i], m_swapChainImageFormat, vk::ImageAspectFlagBits::eColor);
    }
}

void Application::createFramebuffers()
{
    //fill framebuffer vector with swapchain img info
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    for(size_t i = 0; i < m_swapChainImageViews.size(); i++)
    {
        std::array<vk::ImageView, 2> attachments = 
        {
            m_swapChainImageViews[i],
            m_depthImage.getImageView(),
        };

        vk::FramebufferCreateInfo framebufferInfo;
        framebufferInfo.setRenderPass(m_renderPass);
        framebufferInfo.setAttachmentCount(static_cast<uint32_t>(attachments.size()));
        framebufferInfo.setPAttachments(attachments.data());
        framebufferInfo.setWidth(m_swapChainExtent.width);
        framebufferInfo.setHeight(m_swapChainExtent.height);
        framebufferInfo.setLayers(1);

        m_swapChainFramebuffers[i] = m_device.createFramebuffer(framebufferInfo);
    }
}

void Application::createRenderPass()
{
    vk::AttachmentDescription colorAttachment;
    colorAttachment.setFormat(m_swapChainImageFormat);
    colorAttachment.setSamples(vk::SampleCountFlagBits::e1);
    colorAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    colorAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    colorAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colorAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colorAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colorAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentDescription depthAttachment;
    depthAttachment.setFormat(VulkanUtils::findDepthFormat(m_physicalDevice));
    depthAttachment.setSamples(vk::SampleCountFlagBits::e1);
    depthAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    depthAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    depthAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference colorAttachmentRef;
    colorAttachmentRef.setAttachment(0);
    colorAttachmentRef.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference depthAttachmentRef;
    depthAttachmentRef.setAttachment(1);
    depthAttachmentRef.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::SubpassDescription subpass;
    subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    subpass.setColorAttachmentCount(1);
    subpass.setPColorAttachments(&colorAttachmentRef);
    subpass.setPDepthStencilAttachment(&depthAttachmentRef);

    vk::SubpassDependency dependency;
    dependency.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    dependency.setDstSubpass(0);
    dependency.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependency.setSrcAccessMask(vk::AccessFlagBits::eNone);
    dependency.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests);
    dependency.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite);

    std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.setAttachmentCount(static_cast<uint32_t>(attachments.size()));
    renderPassInfo.setPAttachments(attachments.data());
    renderPassInfo.setSubpassCount(1);
    renderPassInfo.setPSubpasses(&subpass);
    renderPassInfo.setDependencyCount(1);
    renderPassInfo.setPDependencies(&dependency);

    m_renderPass = m_device.createRenderPass(renderPassInfo);
}

void Application::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding uboLayoutBinding;
    uboLayoutBinding.setBinding(0);
    uboLayoutBinding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uboLayoutBinding.setDescriptorCount(1);
    uboLayoutBinding.setStageFlags(vk::ShaderStageFlagBits::eVertex);
    uboLayoutBinding.setPImmutableSamplers(nullptr); //optional

    vk::DescriptorSetLayoutBinding samplerLayoutBinding;
    samplerLayoutBinding.setBinding(1);
    samplerLayoutBinding.setDescriptorCount(1);
    samplerLayoutBinding.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    samplerLayoutBinding.setPImmutableSamplers(nullptr);
    samplerLayoutBinding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
    vk::DescriptorSetLayoutCreateInfo layoutInfo;
    layoutInfo.setBindingCount(static_cast<uint32_t>(bindings.size()));
    layoutInfo.setPBindings(bindings.data());

    m_descriptorSetLayout = m_device.createDescriptorSetLayout(layoutInfo);
}

void Application::createGraphicsPipeline()
{
    //create shader modules
    auto vertShaderCode = Utils::readFile("res/shaders/vert.spv");
    auto fragShaderCode = Utils::readFile("res/shaders/frag.spv");
    vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    //create shader info
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
    vk::PipelineShaderStageCreateInfo fragShaderStageInfo;

    //vertex shader info
    vertShaderStageInfo.setStage(vk::ShaderStageFlagBits::eVertex);
    vertShaderStageInfo.setModule(vertShaderModule);
    vertShaderStageInfo.setPName("main");
    vertShaderStageInfo.setPSpecializationInfo(nullptr);

    //fragment shader info
    fragShaderStageInfo.setStage(vk::ShaderStageFlagBits::eFragment);
    fragShaderStageInfo.setModule(fragShaderModule);
    fragShaderStageInfo.setPName("main");
    fragShaderStageInfo.setPSpecializationInfo(nullptr);

    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {vertShaderStageInfo, fragShaderStageInfo};

    //dynamic state
    std::vector<vk::DynamicState> dynamicStates =
    {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };

    vk::PipelineDynamicStateCreateInfo dynamicState;
    dynamicState.setDynamicStateCount(static_cast<uint32_t>(dynamicStates.size()));
    dynamicState.setPDynamicStates(dynamicStates.data());

    //vertex input info
    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
    vertexInputInfo.setVertexBindingDescriptionCount(1);
    vertexInputInfo.setPVertexBindingDescriptions(&bindingDescription);
    vertexInputInfo.setVertexAttributeDescriptionCount(static_cast<uint32_t>(attributeDescriptions.size()));
    vertexInputInfo.setPVertexAttributeDescriptions(attributeDescriptions.data());

    //input assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssembly.setPrimitiveRestartEnable(false);

    //viewport and scissor
    vk::Viewport viewport{};
    viewport.setX(0.f);
    viewport.setY(0.f);
    viewport.setWidth(static_cast<float>(m_swapChainExtent.width));
    viewport.setHeight(static_cast<float>(m_swapChainExtent.height));
    viewport.setMinDepth(0.f);
    viewport.setMaxDepth(1.f);

    vk::Rect2D scissor;
    scissor.setOffset({0, 0});
    scissor.setExtent(m_swapChainExtent);

    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewportCount(1);
    viewportState.setPViewports(&viewport);
    viewportState.setScissorCount(1);
    viewportState.setPScissors(&scissor);

    //rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.setDepthClampEnable(false);
    rasterizer.setRasterizerDiscardEnable(false);
    rasterizer.setPolygonMode(vk::PolygonMode::eFill);
    rasterizer.setLineWidth(1.f);
    rasterizer.setCullMode(vk::CullModeFlagBits::eBack);
    rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterizer.setDepthBiasEnable(false);
    rasterizer.setDepthBiasConstantFactor(0.f);//optional
    rasterizer.setDepthBiasClamp(0.f); //optional
    rasterizer.setDepthBiasSlopeFactor(0.f); //optional

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setSampleShadingEnable(false);
    multisampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);
    multisampling.setMinSampleShading(1.f);//optional
    multisampling.setPSampleMask(nullptr); //optional
    multisampling.setAlphaToCoverageEnable(false); //optional
    multisampling.setAlphaToOneEnable(false); //optional

    //options for alpha blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment;
    colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    colorBlendAttachment.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
    colorBlendAttachment.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
    colorBlendAttachment.setColorBlendOp(vk::BlendOp::eAdd);
    colorBlendAttachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    colorBlendAttachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    colorBlendAttachment.setAlphaBlendOp(vk::BlendOp::eAdd);

    vk::PipelineColorBlendStateCreateInfo colorBlending;
    colorBlending.setLogicOpEnable(false);
    colorBlending.setLogicOp(vk::LogicOp::eCopy); //optional
    colorBlending.setAttachmentCount(1);
    colorBlending.setPAttachments(&colorBlendAttachment);
    colorBlending.setBlendConstants({0.f, 0.f, 0.f, 0.f}); //optional

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.setSetLayoutCount(1);
    pipelineLayoutInfo.setPSetLayouts(&m_descriptorSetLayout);
    pipelineLayoutInfo.setPushConstantRangeCount(0); //optional
    pipelineLayoutInfo.setPPushConstantRanges(nullptr);//optional

    vk::PipelineDepthStencilStateCreateInfo depthStencil;
    depthStencil.setDepthTestEnable(true);
    depthStencil.setDepthWriteEnable(true);
    depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
    depthStencil.setDepthBoundsTestEnable(false);
    depthStencil.setMinDepthBounds(0.f); //optional
    depthStencil.setMaxDepthBounds(1.f); //optional
    depthStencil.setStencilTestEnable(false);
    depthStencil.setFront({}); //optional
    depthStencil.setBack({}); //optional

    m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutInfo);

    //create pipeline from all infos
    vk::GraphicsPipelineCreateInfo pipelineInfo;
    pipelineInfo.setStageCount(shaderStages.size());
    pipelineInfo.setPStages(shaderStages.data());
    pipelineInfo.setPVertexInputState(&vertexInputInfo);
    pipelineInfo.setPInputAssemblyState(&inputAssembly);
    pipelineInfo.setPViewportState(&viewportState);
    pipelineInfo.setPRasterizationState(&rasterizer);
    pipelineInfo.setPMultisampleState(&multisampling);
    pipelineInfo.setPDepthStencilState(&depthStencil);
    pipelineInfo.setPColorBlendState(&colorBlending);
    pipelineInfo.setPDynamicState(&dynamicState);
    pipelineInfo.setLayout(m_pipelineLayout);
    pipelineInfo.setRenderPass(m_renderPass);
    pipelineInfo.setSubpass(0);
    pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE); //optional
    pipelineInfo.setBasePipelineIndex(-1); //optional

    auto pipelineCreationResult = m_device.createGraphicsPipeline(VK_NULL_HANDLE, pipelineInfo);

    if(pipelineCreationResult.result != vk::Result::eSuccess)
        throw std::runtime_error("failed to create graphics pipeline");

    m_graphicsPipeline = pipelineCreationResult.value;
    //delete shader modules
    m_device.destroyShaderModule(vertShaderModule);
    m_device.destroyShaderModule(fragShaderModule);
}

void Application::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = VulkanUtils::findQueueFamilies(m_physicalDevice, m_surface);

    vk::CommandPoolCreateInfo poolInfo;
    poolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
    poolInfo.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());

    m_commandPool = m_device.createCommandPool(poolInfo);

    Renderer::Vulkan::RenderCommand::initRenderCommands(m_device, m_commandPool, m_graphicsQueue);
}

void Application::createCommandBuffers()
{
    m_commandBuffers.resize(m_maxFramesInFlight);

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.setCommandPool(m_commandPool);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
    allocInfo.setCommandBufferCount(static_cast<uint32_t>(m_commandBuffers.size()));

    m_commandBuffers = m_device.allocateCommandBuffers(allocInfo);
}

void Application::recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
    //begin recording command buffer 
    vk::CommandBufferBeginInfo beginInfo;
    beginInfo.setPInheritanceInfo(nullptr);

    commandBuffer.begin(beginInfo);

    //fill out render pass info
    vk::ClearColorValue clearColor;
    clearColor.setFloat32({0.f, 0.f, 0.f, 1.f});

    std::array<vk::ClearValue, 2> clearValues{};
    clearValues[0].setColor(clearColor);
    clearValues[1].setDepthStencil({1.f, 0});

    vk::Rect2D renderArea;
    renderArea.setOffset({0, 0});
    renderArea.setExtent(m_swapChainExtent);

    vk::RenderPassBeginInfo renderPassInfo;
    renderPassInfo.setRenderPass(m_renderPass);
    renderPassInfo.setFramebuffer(m_swapChainFramebuffers[imageIndex]);
    renderPassInfo.setRenderArea(renderArea);
    renderPassInfo.setClearValueCount(static_cast<uint32_t>(clearValues.size()));
    renderPassInfo.setPClearValues(clearValues.data());

    commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphicsPipeline);

    //viewport and scissor are dynamic so have to set each time
    vk::Viewport viewport;
    viewport.setX(0.f);
    viewport.setY(0.f);
    viewport.setWidth(static_cast<float>(m_swapChainExtent.width));
    viewport.setHeight(static_cast<float>(m_swapChainExtent.height));
    viewport.setMinDepth(0.f);
    viewport.setMaxDepth(1.f);
    commandBuffer.setViewport(0, 1, &viewport);

    vk::Rect2D scissor;
    scissor.setOffset({0, 0});
    scissor.setExtent(m_swapChainExtent);
    commandBuffer.setScissor(0, 1, &scissor);
    
    //buffers
    vk::Buffer vertexBuffers[] = {m_vertexBuffer.getHandle() };
    vk::DeviceSize offsets[] = {0};
    commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(m_indexBuffer.getHandle(), 0, vk::IndexType::eUint32);

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSets[m_currentFrame], 0, nullptr);

    //draw
    commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    //end recording
    commandBuffer.endRenderPass();

    commandBuffer.end();
}

void Application::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(m_maxFramesInFlight);
    m_renderFinishedSemaphores.resize(m_maxFramesInFlight);
    m_inFlightFences.resize(m_maxFramesInFlight);

    vk::SemaphoreCreateInfo semaphoreInfo;

    vk::FenceCreateInfo fenceInfo;
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    for(int i = 0; i < m_maxFramesInFlight; i++)
    {
        m_imageAvailableSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
        m_renderFinishedSemaphores[i] = m_device.createSemaphore(semaphoreInfo);
        m_inFlightFences[i] = m_device.createFence(fenceInfo);
    }
}

void Application::createVertexBuffer()
{
    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    Renderer::Vulkan::Buffer stagingBuff(m_device, m_physicalDevice);
    stagingBuff.create(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, 
                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    stagingBuff.allocateAndMap<Vertex>(vertices);

    m_vertexBuffer.create(bufferSize,
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

    m_vertexBuffer.copyBuffer(stagingBuff);
    stagingBuff.free();
}

void Application::createIndexBuffer()
{
    //same as vertex buffer
    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    Renderer::Vulkan::Buffer stagingBuff(m_device, m_physicalDevice);
    stagingBuff.create(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    stagingBuff.allocateAndMap<uint32_t>(indices);

    m_indexBuffer.create(bufferSize,
                          vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                          vk::MemoryPropertyFlagBits::eDeviceLocal);

    m_indexBuffer.copyBuffer(stagingBuff);
    stagingBuff.free();
}

void Application::createUniformBuffers()
{
    vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

    m_uniformBuffers.resize(m_maxFramesInFlight);
    for(size_t i = 0; i < m_maxFramesInFlight; i++)
    {
        m_uniformBuffers[i].setDevices(m_device, m_physicalDevice);
        m_uniformBuffers[i].create(bufferSize,
                       vk::BufferUsageFlagBits::eUniformBuffer,
                       vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    }
}

void Application::createDescriptorPool()
{
    std::array<vk::DescriptorPoolSize, 2> poolSizes;
    poolSizes[0].setType(vk::DescriptorType::eUniformBuffer);
    poolSizes[0].setDescriptorCount(m_maxFramesInFlight);
    poolSizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
    poolSizes[1].setDescriptorCount(m_maxFramesInFlight);

    vk::DescriptorPoolCreateInfo poolInfo;
    poolInfo.setPoolSizeCount(static_cast<uint32_t>(poolSizes.size()));
    poolInfo.setPPoolSizes(poolSizes.data());
    poolInfo.setMaxSets(m_maxFramesInFlight);

    m_descriptorPool = m_device.createDescriptorPool(poolInfo);
}

void Application::createDescriptorSets()
{
    std::vector<vk::DescriptorSetLayout> layouts(m_maxFramesInFlight, m_descriptorSetLayout);
    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.setDescriptorPool(m_descriptorPool);
    allocInfo.setDescriptorSetCount(m_maxFramesInFlight);
    allocInfo.setPSetLayouts(layouts.data());

    m_descriptorSets = m_device.allocateDescriptorSets(allocInfo);

    for(size_t i = 0; i < m_maxFramesInFlight; i++)
    {
        vk::DescriptorBufferInfo bufferInfo;
        bufferInfo.setBuffer(m_uniformBuffers[i].getHandle());
        bufferInfo.setOffset(0);
        bufferInfo.setRange(sizeof(UniformBufferObject));

        vk::DescriptorImageInfo imageInfo;
        imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
        imageInfo.setImageView(m_texture.getImageView());
        imageInfo.setSampler(m_texture.getSampler());

        std::array<vk::WriteDescriptorSet, 2> descriptorWrites;
        descriptorWrites[0].setDstSet(m_descriptorSets[i]);
        descriptorWrites[0].setDstBinding(0);
        descriptorWrites[0].setDstArrayElement(0);
        descriptorWrites[0].setDescriptorType(vk::DescriptorType::eUniformBuffer);
        descriptorWrites[0].setDescriptorCount(1);
        descriptorWrites[0].setPBufferInfo(&bufferInfo);

        descriptorWrites[1].setDstSet(m_descriptorSets[i]);
        descriptorWrites[1].setDstBinding(1);
        descriptorWrites[1].setDstArrayElement(0);
        descriptorWrites[1].setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        descriptorWrites[1].setDescriptorCount(1);
        descriptorWrites[1].setPImageInfo(&imageInfo);

        m_device.updateDescriptorSets(descriptorWrites, {});
    }
}

void Application::createTextureImage()
{
    m_texture.create("res/textures/test.png", vk::Filter::eNearest, vk::SamplerAddressMode::eRepeat);
}

void Application::createDepthResources()
{
    vk::Format depthFormat = VulkanUtils::findDepthFormat(m_physicalDevice);
    m_depthImage.setSize(m_swapChainExtent.width, m_swapChainExtent.height);
    m_depthImage.create(depthFormat,
                              vk::ImageTiling::eOptimal, 
                              vk::ImageUsageFlagBits::eDepthStencilAttachment, 
                              vk::MemoryPropertyFlagBits::eDeviceLocal,
                              vk::ImageAspectFlagBits::eDepth);
    m_depthImage.transitionImageLayout(depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

vk::ShaderModule Application::createShaderModule(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo createInfo{};
    createInfo.setCodeSize(code.size());
    createInfo.setPCode(reinterpret_cast<const uint32_t*>(code.data()));

    return m_device.createShaderModule(createInfo);
}
