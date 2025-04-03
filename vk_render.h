#ifndef VK_RENDER_H
#define VK_RENDER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h> // for uint32_t
#include "vulkan/vulkan.h"
#include "vulkan/vulkan_win32.h"
    // --------------------------------------------------------------------------------
    // Public API: Structures and function prototypes
    // --------------------------------------------------------------------------------

    // Initializes the Vulkan renderer on a Win32 window (HWND) with the given width and height.
    // Returns 0 on success; nonzero on failure.
    int vk_render_init(void *windowHandle, int width, int height);

    // Pass a Model-View-Projection (MVP) matrix to the Vulkan code. The matrix must be a 4x4 array
    // of floats (in column-major order, as expected by GLSL by default) that transforms your mesh from
    // model space to clip space.
    void vk_render_set_mvp(const float mvp[16]);

    // Performs a single render pass: it records and submits command buffers that draw the current mesh.
    void vk_render_draw();

    // Releases all Vulkan resources created by vk_render_init() and related functions.
    void vk_render_cleanup(void);

    // --------------------------------------------------------------------------------
    // New Mesh Management API
    // --------------------------------------------------------------------------------
    // Creates a mesh: allocates Vulkan buffers for the provided mesh data and returns a unique handle.
    uint32_t vk_render_create_mesh(Mesh *mesh_data);

    // Destroys a mesh associated with the given handle, deallocating its Vulkan resources.
    void vk_render_destroy_mesh(uint32_t handle);

    // Updates the data in the Vulkan buffers associated with the given mesh handle.
    void vk_render_set_mesh(uint32_t mesh_handle, const Mesh *mesh);

    // Renders the mesh associated with the given handle on this frame only.
    void vk_render_mesh(uint32_t handle);

#ifdef __cplusplus
}
#endif

// --------------------------------------------------------------------------------
// Implementation (define VK_RENDER_IMPLEMENTATION in one source file to include it)
// --------------------------------------------------------------------------------
// #ifdef VK_RENDER_IMPLEMENTATION

// ---------- Standard headers ----------
#include <windows.h>
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "io.h"

// Include C++ headers for the mesh tracking
#include <unordered_map>
#include <cstdint>

// ---------- Internal structures and global variables ----------
// All Vulkan objects are stored in static globals, so that user code never sees them.
static VkInstance g_instance = VK_NULL_HANDLE;
static VkSurfaceKHR g_surface = VK_NULL_HANDLE;
static VkPhysicalDevice g_physDevice = VK_NULL_HANDLE;
static VkDevice g_device = VK_NULL_HANDLE;
static VkQueue g_graphicsQueue = VK_NULL_HANDLE;
static unsigned int g_graphicsQueueFamilyIndex = 0;
static VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;
static VkFormat g_swapchainFormat;
static unsigned int g_swapchainImageCount = 0;
static VkImage *g_swapchainImages = NULL;
static VkImageView *g_swapchainImageViews = NULL;
static VkRenderPass g_renderPass = VK_NULL_HANDLE;
static VkPipelineLayout g_pipelineLayout = VK_NULL_HANDLE;
static VkPipeline g_graphicsPipeline = VK_NULL_HANDLE;
static VkFramebuffer *g_framebuffers = NULL;
static unsigned int g_framebufferCount = 0;
static VkCommandPool g_commandPool = VK_NULL_HANDLE;
static VkCommandBuffer *g_commandBuffers = NULL;
static VkSemaphore g_imageAvailableSemaphore = VK_NULL_HANDLE;
static VkSemaphore g_renderFinishedSemaphore = VK_NULL_HANDLE;

static std::vector<uint32_t> g_renderMeshHandles;

// Uniform buffer for the MVP matrix (4x4 float = 16 floats)
static VkBuffer g_uniformBuffer = VK_NULL_HANDLE;
static VkDeviceMemory g_uniformBufferMemory = VK_NULL_HANDLE;

// Store window and dimensions (for swapchain and viewport creation)
static HWND vk_hwnd = 0;
static int g_width = 800;
static int g_height = 600;

// ---------- Utility function for error handling ----------
static void checkVkResult(VkResult result, const char *msg)
{
    if (result != VK_SUCCESS)
    {
        fprintf(stderr, "Vulkan error: %s (code %d)\n", msg, result);
        exit(-1);
    }
}

// ---------- Internal helper functions ----------

static void CreateInstance()
{
    VkApplicationInfo appInfo;
    memset(&appInfo, 0, sizeof(appInfo));
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Renderer";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    const char *extensions[] = {"VK_KHR_surface", "VK_KHR_win32_surface"};

    VkInstanceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = 2;
    createInfo.ppEnabledExtensionNames = extensions;

    VkResult result = vkCreateInstance(&createInfo, NULL, &g_instance);
    checkVkResult(result, "Failed to create Vulkan instance");
}

static void CreateSurface()
{
    VkWin32SurfaceCreateInfoKHR createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandle(NULL);
    createInfo.hwnd = vk_hwnd;
    VkResult result = vkCreateWin32SurfaceKHR(g_instance, &createInfo, NULL, &g_surface);
    checkVkResult(result, "Failed to create Win32 surface");
}

static void PickPhysicalDevice()
{
    unsigned int deviceCount = 0;
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, NULL);
    if (deviceCount == 0)
    {
        fprintf(stderr, "Failed to find GPUs with Vulkan support.\n");
        exit(-1);
    }
    VkPhysicalDevice devices[16];
    vkEnumeratePhysicalDevices(g_instance, &deviceCount, devices);
    for (unsigned int i = 0; i < deviceCount; i++)
    {
        unsigned int queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, NULL);
        VkQueueFamilyProperties queueProps[16];
        vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, queueProps);
        for (unsigned int j = 0; j < queueFamilyCount; j++)
        {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, g_surface, &presentSupport);
            if ((queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport)
            {
                g_physDevice = devices[i];
                g_graphicsQueueFamilyIndex = j;
                return;
            }
        }
    }
    fprintf(stderr, "Failed to find a suitable GPU.\n");
    exit(-1);
}

static void CreateLogicalDevice()
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo;
    memset(&queueCreateInfo, 0, sizeof(queueCreateInfo));
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = g_graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const char *deviceExtensions[] = {"VK_KHR_swapchain"};

    VkDeviceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = deviceExtensions;

    VkResult result = vkCreateDevice(g_physDevice, &createInfo, NULL, &g_device);
    checkVkResult(result, "Failed to create logical device");
    vkGetDeviceQueue(g_device, g_graphicsQueueFamilyIndex, 0, &g_graphicsQueue);
}

static void CreateSwapchain()
{
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_physDevice, g_surface, &capabilities);
    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == 0xFFFFFFFF)
    {
        extent.width = g_width;
        extent.height = g_height;
    }
    else
    {
        g_width = extent.width;
        g_height = extent.height;
    }

    VkSwapchainCreateInfoKHR swapchainInfo;
    memset(&swapchainInfo, 0, sizeof(swapchainInfo));
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = g_surface;
    swapchainInfo.minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && swapchainInfo.minImageCount > capabilities.maxImageCount)
        swapchainInfo.minImageCount = capabilities.maxImageCount;
    swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    g_swapchainFormat = swapchainInfo.imageFormat;
    swapchainInfo.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;
    VkResult result = vkCreateSwapchainKHR(g_device, &swapchainInfo, NULL, &g_swapchain);
    checkVkResult(result, "Failed to create swapchain");

    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, NULL);
    g_swapchainImages = (VkImage *)malloc(sizeof(VkImage) * g_swapchainImageCount);
    vkGetSwapchainImagesKHR(g_device, g_swapchain, &g_swapchainImageCount, g_swapchainImages);

    g_swapchainImageViews = (VkImageView *)malloc(sizeof(VkImageView) * g_swapchainImageCount);
    for (unsigned int i = 0; i < g_swapchainImageCount; i++)
    {
        VkImageViewCreateInfo viewInfo;
        memset(&viewInfo, 0, sizeof(viewInfo));
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = g_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = g_swapchainFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(g_device, &viewInfo, NULL, &g_swapchainImageViews[i]);
        checkVkResult(result, "Failed to create image view");
    }
}

static void CreateRenderPass()
{
    VkAttachmentDescription colorAttachment;
    memset(&colorAttachment, 0, sizeof(colorAttachment));
    colorAttachment.format = g_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef;
    memset(&colorAttachmentRef, 0, sizeof(colorAttachmentRef));
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    memset(&subpass, 0, sizeof(subpass));
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo;
    memset(&renderPassInfo, 0, sizeof(renderPassInfo));
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VkResult result = vkCreateRenderPass(g_device, &renderPassInfo, NULL, &g_renderPass);
    checkVkResult(result, "Failed to create render pass");
}

static void CreateGraphicsPipeline()
{
    // Load vertex and fragment shader SPIR-V code
    uint32_t vertex_shader_len_bytes = 0;
    uint32_t *vertex_shader_code = read_file("shaders\\vert.spv", &vertex_shader_len_bytes);
    uint32_t fragment_shader_len_bytes = 0;
    uint32_t *fragment_shader_code = read_file("shaders\\frag.spv", &fragment_shader_len_bytes);

    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = vertex_shader_len_bytes;
    shaderModuleCreateInfo.pCode = vertex_shader_code;
    VkShaderModule vertShaderModule;
    VkResult result = vkCreateShaderModule(g_device, &shaderModuleCreateInfo, NULL, &vertShaderModule);
    checkVkResult(result, "Failed to create vertex shader module");

    memset(&shaderModuleCreateInfo, 0, sizeof(shaderModuleCreateInfo));
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.codeSize = fragment_shader_len_bytes;
    shaderModuleCreateInfo.pCode = fragment_shader_code;
    VkShaderModule fragShaderModule;
    result = vkCreateShaderModule(g_device, &shaderModuleCreateInfo, NULL, &fragShaderModule);
    checkVkResult(result, "Failed to create fragment shader module");

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertShaderModule;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragShaderModule;
    shaderStages[1].pName = "main";

    // Update vertex input: one binding with 4 floats per vertex (vec4 position)
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(float) * 4;
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributeDescription = {};
    attributeDescription.binding = 0;
    attributeDescription.location = 0;
    attributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescription.offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)g_width;
    viewport.height = (float)g_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = g_width;
    scissor.extent.height = g_height;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Create a descriptor set layout for the MVP uniform buffer (binding = 0)
    VkDescriptorSetLayoutBinding mvpLayoutBinding = {};
    mvpLayoutBinding.binding = 0;
    mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    mvpLayoutBinding.descriptorCount = 1;
    mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    mvpLayoutBinding.pImmutableSamplers = NULL;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo = {};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &mvpLayoutBinding;

    VkDescriptorSetLayout descriptorSetLayout;
    result = vkCreateDescriptorSetLayout(g_device, &descriptorLayoutInfo, NULL, &descriptorSetLayout);
    checkVkResult(result, "Failed to create descriptor set layout for MVP uniform");

    // Update pipeline layout to include the descriptor set layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    VkResult pipelineResult = vkCreatePipelineLayout(g_device, &pipelineLayoutInfo, NULL, &g_pipelineLayout);
    checkVkResult(pipelineResult, "Failed to create pipeline layout");

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = g_pipelineLayout;
    pipelineInfo.renderPass = g_renderPass;
    pipelineInfo.subpass = 0;
    VkResult graphicsResult = vkCreateGraphicsPipelines(g_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &g_graphicsPipeline);
    checkVkResult(graphicsResult, "Failed to create graphics pipeline");

    vkDestroyShaderModule(g_device, vertShaderModule, NULL);
    vkDestroyShaderModule(g_device, fragShaderModule, NULL);
    // Note: The descriptor set layout should be destroyed during cleanup when it is no longer needed.
    // vkDestroyDescriptorSetLayout(g_device, descriptorSetLayout, NULL);
}

static void CreateFramebuffers()
{
    g_framebufferCount = g_swapchainImageCount;
    g_framebuffers = (VkFramebuffer *)malloc(sizeof(VkFramebuffer) * g_framebufferCount);
    for (unsigned int i = 0; i < g_framebufferCount; i++)
    {
        VkImageView attachments[1] = {g_swapchainImageViews[i]};
        VkFramebufferCreateInfo fbInfo;
        memset(&fbInfo, 0, sizeof(fbInfo));
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = g_renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = g_width;
        fbInfo.height = g_height;
        fbInfo.layers = 1;
        VkResult result = vkCreateFramebuffer(g_device, &fbInfo, NULL, &g_framebuffers[i]);
        checkVkResult(result, "Failed to create framebuffer");
    }
}

static void CreateCommandPoolAndBuffers()
{
    VkCommandPoolCreateInfo poolInfo;
    memset(&poolInfo, 0, sizeof(poolInfo));
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = g_graphicsQueueFamilyIndex;
    VkResult result = vkCreateCommandPool(g_device, &poolInfo, NULL, &g_commandPool);
    checkVkResult(result, "Failed to create command pool");

    g_commandBuffers = (VkCommandBuffer *)malloc(sizeof(VkCommandBuffer) * g_framebufferCount);
    VkCommandBufferAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = g_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = g_framebufferCount;
    result = vkAllocateCommandBuffers(g_device, &allocInfo, g_commandBuffers);
    checkVkResult(result, "Failed to allocate command buffers");
}

static void CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo;
    memset(&semaphoreInfo, 0, sizeof(semaphoreInfo));
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkResult result = vkCreateSemaphore(g_device, &semaphoreInfo, NULL, &g_imageAvailableSemaphore);
    checkVkResult(result, "Failed to create image available semaphore");
    result = vkCreateSemaphore(g_device, &semaphoreInfo, NULL, &g_renderFinishedSemaphore);
    checkVkResult(result, "Failed to create render finished semaphore");
}

static void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
    VkBufferCreateInfo bufferInfo;
    memset(&bufferInfo, 0, sizeof(bufferInfo));
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(g_device, &bufferInfo, NULL, buffer);
    checkVkResult(result, "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(g_device, *buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(allocInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(g_physDevice, &memProperties);
    unsigned int i;
    for (i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            ((memProperties.memoryTypes[i].propertyFlags & properties) == properties))
        {
            allocInfo.memoryTypeIndex = i;
            break;
        }
    }
    if (i == memProperties.memoryTypeCount)
    {
        fprintf(stderr, "Failed to find suitable memory type.\n");
        exit(-1);
    }
    result = vkAllocateMemory(g_device, &allocInfo, NULL, bufferMemory);
    checkVkResult(result, "Failed to allocate buffer memory");
    vkBindBufferMemory(g_device, *buffer, *bufferMemory, 0);
}

// --------------------------------------------------------------------------------
// Public API implementations
// --------------------------------------------------------------------------------

// Updated createMVPBuffer function
// Creates the MVP uniform buffer and maps it to host memory.
// Returns true on success, false on failure.
bool createMVPBuffer(VkPhysicalDevice physicalDevice, VkDevice logicalDevice)
{
    VkDeviceSize bufferSize = sizeof(MVPMatrix);

    if (!createBuffer(physicalDevice, logicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      mvpBuffer, mvpBufferMemory))
    {
        // Error handling: Buffer creation failed.
        return false;
    }

    // Map the buffer memory so we can update the MVP matrix every frame.
    if (vkMapMemory(logicalDevice, mvpBufferMemory, 0, bufferSize, 0, &mappedMemory) != VK_SUCCESS)
    {
        // Error handling: Mapping failed.
        return false;
    }

    return true;
}

int vk_render_init(void *windowHandle, int width, int height)
{
    vk_hwnd = (HWND)windowHandle;
    g_width = width;
    g_height = height;

    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPoolAndBuffers();
    CreateSyncObjects();

    // Create uniform buffer for the MVP matrix (16 floats).
    VkDeviceSize bufferSize = sizeof(float) * 16;
    CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &g_uniformBuffer, &g_uniformBufferMemory);

    if (!createMVPBuffer(physicalDevice, logicalDevice))
    {
        // Error handling: Failed to create the MVP buffer
        return false;
    }

    return 0;
}

struct MVPMatrix
{
    float MVP[16];
};

// Updated vk_render_set_mvp function
// This function copies the provided MVP matrix into the mapped uniform buffer memory.
// It assumes that the MVP buffer has been successfully created and mapped.
void vk_render_set_mvp(const MVPMatrix &mvp)
{
    if (mappedMemory == nullptr)
    {
        // Error handling: The uniform buffer memory is not mapped.
        // Depending on your error logging framework, you could log an error here.
        return;
    }
    // Copy the new MVP data to the uniform buffer.
    memcpy(mappedMemory, &mvp, sizeof(MVPMatrix));
}
// Structure to hold per–mesh Vulkan objects and counts.
struct MeshBuffer
{
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    uint32_t vertexCount;
    uint32_t indexCount;
};

// Global unordered_map to track mesh handles to MeshBuffer.
static std::unordered_map<uint32_t, MeshBuffer> g_meshMap;
// Global counter for unique mesh handles.
static uint32_t g_nextMeshHandle = 1;

// -----------------------------------------------------------------------------
// Updated RecordCommandBuffers() using the new mesh list.
// -----------------------------------------------------------------------------
// This function now iterates over the list of mesh handles generated by repeated
// calls to vk_render_mesh(), and records draw commands for each mesh into every
// command buffer (one per framebuffer).
static void RecordCommandBuffers()
{
    for (unsigned int i = 0; i < g_framebufferCount; i++)
    {
        VkCommandBuffer cmdBuffer = g_commandBuffers[i];
        VkCommandBufferBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VkResult result = vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        checkVkResult(result, "Failed to begin command buffer");

        VkClearValue clearColor = {.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo;
        memset(&renderPassInfo, 0, sizeof(renderPassInfo));
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g_renderPass;
        renderPassInfo.framebuffer = g_framebuffers[i];
        renderPassInfo.renderArea.offset.x = 0;
        renderPassInfo.renderArea.offset.y = 0;
        renderPassInfo.renderArea.extent.width = g_width;
        renderPassInfo.renderArea.extent.height = g_height;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // Begin the render pass.
        vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        // Bind the graphics pipeline.
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_graphicsPipeline);

        // Iterate over each mesh handle queued for rendering.
        for (uint32_t handle : g_renderMeshHandles)
        {
            auto it = g_meshMap.find(handle);
            if (it == g_meshMap.end())
            {
                fprintf(stderr, "Mesh handle %u not found in mesh map. Skipping.\n", handle);
                continue;
            }
            const MeshBuffer &mb = it->second;

            VkBuffer vertexBuffers[] = {mb.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

            if (mb.indexBuffer != VK_NULL_HANDLE)
            {
                vkCmdBindIndexBuffer(cmdBuffer, mb.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmdBuffer, mb.indexCount, 1, 0, 0, 0);
            }
            else
            {
                vkCmdDraw(cmdBuffer, mb.vertexCount, 1, 0, 0);
            }
        }

        vkCmdEndRenderPass(cmdBuffer);
        result = vkEndCommandBuffer(cmdBuffer);
        checkVkResult(result, "Failed to record command buffer");
    }
}

// -----------------------------------------------------------------------------
// Updated vk_render_draw() function
// -----------------------------------------------------------------------------
// This function now uses the list of meshes collected from vk_render_mesh() calls,
// updates the command buffers to render only those meshes, and performs the
// rendering. Finally, it clears the mesh list for the next frame.
void vk_render_draw(void)
{
    // Update command buffers with the selected meshes.
    RecordCommandBuffers();

    // Acquire the next swapchain image.
    uint32_t imageIndex;
    vkAcquireNextImageKHR(g_device, g_swapchain, 0xFFFFFFFF, g_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    VkSubmitInfo submitInfo;
    memset(&submitInfo, 0, sizeof(submitInfo));
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {g_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &g_commandBuffers[imageIndex];
    VkSemaphore signalSemaphores[] = {g_renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    VkResult result = vkQueueSubmit(g_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    checkVkResult(result, "Failed to submit draw command");

    VkPresentInfoKHR presentInfo;
    memset(&presentInfo, 0, sizeof(presentInfo));
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &g_swapchain;
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(g_graphicsQueue, &presentInfo);
    checkVkResult(result, "Failed to present swapchain image");

    vkQueueWaitIdle(g_graphicsQueue);

    // Clear the mesh list for the next frame.
    g_renderMeshHandles.clear();
}

void vk_render_cleanup(void)
{
    // Clean up the uniform buffer.
    vkDestroyBuffer(g_device, g_uniformBuffer, NULL);
    vkFreeMemory(g_device, g_uniformBufferMemory, NULL);

    // Iterate over each mesh in the mesh map and free its Vulkan resources.
    for (auto &pair : g_meshMap)
    {
        MeshBuffer &mb = pair.second;
        vkDestroyBuffer(g_device, mb.vertexBuffer, NULL);
        vkFreeMemory(g_device, mb.vertexBufferMemory, NULL);
        if (mb.indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(g_device, mb.indexBuffer, NULL);
            vkFreeMemory(g_device, mb.indexBufferMemory, NULL);
        }
    }
    // Clear the mesh map.
    g_meshMap.clear();

    // Clean up other Vulkan objects.
    vkDestroySemaphore(g_device, g_renderFinishedSemaphore, NULL);
    vkDestroySemaphore(g_device, g_imageAvailableSemaphore, NULL);
    vkDestroyCommandPool(g_device, g_commandPool, NULL);

    for (unsigned int i = 0; i < g_framebufferCount; i++)
    {
        vkDestroyFramebuffer(g_device, g_framebuffers[i], NULL);
    }
    free(g_framebuffers);
    vkDestroyPipeline(g_device, g_graphicsPipeline, NULL);
    vkDestroyPipelineLayout(g_device, g_pipelineLayout, NULL);
    vkDestroyRenderPass(g_device, g_renderPass, NULL);

    for (unsigned int i = 0; i < g_swapchainImageCount; i++)
    {
        vkDestroyImageView(g_device, g_swapchainImageViews[i], NULL);
    }
    free(g_swapchainImageViews);
    free(g_swapchainImages);
    vkDestroySwapchainKHR(g_device, g_swapchain, NULL);
    vkDestroyDevice(g_device, NULL);
    vkDestroySurfaceKHR(g_instance, g_surface, NULL);
    vkDestroyInstance(g_instance, NULL);
}

// --------------------------------------------------------------------------------
// Mesh Management Implementation
// --------------------------------------------------------------------------------

uint32_t vk_render_create_mesh(Mesh *mesh_data)
{
    if (!mesh_data || !mesh_data->vertices || mesh_data->vertexCount == 0)
    {
        fprintf(stderr, "Invalid mesh data.\n");
        return 0;
    }

    MeshBuffer mb = {};
    mb.vertexCount = mesh_data->vertexCount;
    mb.indexCount = mesh_data->indexCount;

    // Create and fill vertex buffer.
    VkDeviceSize vertexBufferSize = sizeof(float) * 3 * mesh_data->vertexCount;
    CreateBuffer(vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 &mb.vertexBuffer, &mb.vertexBufferMemory);
    void *data;
    vkMapMemory(g_device, mb.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, mesh_data->vertices, (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, mb.vertexBufferMemory);

    // Create and fill index buffer if provided.
    if (mesh_data->indices && mesh_data->indexCount > 0)
    {
        VkDeviceSize indexBufferSize = sizeof(unsigned int) * mesh_data->indexCount;
        CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &mb.indexBuffer, &mb.indexBufferMemory);
        vkMapMemory(g_device, mb.indexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, mesh_data->indices, (size_t)indexBufferSize);
        vkUnmapMemory(g_device, mb.indexBufferMemory);
    }
    else
    {
        mb.indexBuffer = VK_NULL_HANDLE;
        mb.indexBufferMemory = VK_NULL_HANDLE;
    }

    // Generate a unique handle and store the MeshBuffer in the map.
    uint32_t handle = g_nextMeshHandle++;
    g_meshMap[handle] = mb;
    return handle;
}

void vk_render_destroy_mesh(uint32_t handle)
{
    auto it = g_meshMap.find(handle);
    if (it == g_meshMap.end())
    {
        fprintf(stderr, "Mesh handle %u not found.\n", handle);
        return;
    }
    MeshBuffer &mb = it->second;
    vkDestroyBuffer(g_device, mb.vertexBuffer, NULL);
    vkFreeMemory(g_device, mb.vertexBufferMemory, NULL);
    if (mb.indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(g_device, mb.indexBuffer, NULL);
        vkFreeMemory(g_device, mb.indexBufferMemory, NULL);
    }
    g_meshMap.erase(it);
}

void vk_render_set_mesh(uint32_t mesh_handle, const Mesh *mesh)
{
    auto it = g_meshMap.find(mesh_handle);
    if (it == g_meshMap.end())
    {
        fprintf(stderr, "Mesh handle %u not found.\n", mesh_handle);
        return;
    }
    if (!mesh || !mesh->vertices || mesh->vertexCount == 0)
    {
        fprintf(stderr, "Invalid mesh data.\n");
        return;
    }
    MeshBuffer &mb = it->second;
    VkDeviceSize vertexBufferSize = sizeof(float) * 3 * mesh->vertexCount;
    void *data;
    // Update vertex buffer data.
    vkMapMemory(g_device, mb.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, mesh->vertices, (size_t)vertexBufferSize);
    vkUnmapMemory(g_device, mb.vertexBufferMemory);
    mb.vertexCount = mesh->vertexCount;

    // Update or create index buffer data if provided.
    if (mesh->indices && mesh->indexCount > 0)
    {
        VkDeviceSize indexBufferSize = sizeof(unsigned int) * mesh->indexCount;
        if (mb.indexBuffer == VK_NULL_HANDLE)
        {
            // Create a new index buffer if one does not exist.
            CreateBuffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         &mb.indexBuffer, &mb.indexBufferMemory);
        }
        vkMapMemory(g_device, mb.indexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, mesh->indices, (size_t)indexBufferSize);
        vkUnmapMemory(g_device, mb.indexBufferMemory);
        mb.indexCount = mesh->indexCount;
    }
    else
    {
        // If no indices are provided, destroy any existing index buffer.
        if (mb.indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(g_device, mb.indexBuffer, NULL);
            vkFreeMemory(g_device, mb.indexBufferMemory, NULL);
            mb.indexBuffer = VK_NULL_HANDLE;
            mb.indexBufferMemory = VK_NULL_HANDLE;
            mb.indexCount = 0;
        }
    }
}

// Simply adds the provided mesh handle to a global list. This list will be used
// later when recording command buffers for rendering the frame.
void vk_render_mesh(uint32_t handle)
{
    // Validate the handle exists in the mesh map before adding.
    if (g_meshMap.find(handle) == g_meshMap.end())
    {
        fprintf(stderr, "Mesh handle %u not found. Ignoring vk_render_mesh() call.\n", handle);
        return;
    }
    g_renderMeshHandles.push_back(handle);
}
// #endif // VK_RENDER_IMPLEMENTATION
#endif // VK_RENDER_H
