#ifndef MOLTENVK_RENDER_H
#define MOLTENVK_RENDER_H

#ifdef __APPLE__

// Standard headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <set>
#include <array>
#include <fstream>

// MoltenVK/Vulkan headers
#include <vulkan/vulkan.h>
#include "io.h"

// Import our math library
#include "math_linear.h"
#include "camera.h"
#include "types.h"

// Forward declarations
struct Mesh;

// Define VK_MVK_macos_surface if not defined
#ifndef VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#define VK_MVK_MACOS_SURFACE_EXTENSION_NAME "VK_MVK_macos_surface"
#endif

// Define structure for macOS surface creation
typedef struct VkMacOSSurfaceCreateInfoMVK {
    VkStructureType                 sType;
    const void*                     pNext;
    VkFlags                         flags;
    const void*                     pView;
} VkMacOSSurfaceCreateInfoMVK;

// Define function pointer type for macOS surface creation
typedef VkResult (VKAPI_PTR *PFN_vkCreateMacOSSurfaceMVK)(
    VkInstance                                  instance,
    const VkMacOSSurfaceCreateInfoMVK*         pCreateInfo,
    const VkAllocationCallbacks*               pAllocator,
    VkSurfaceKHR*                              pSurface);

// Define function pointer for macOS surface creation
static PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;

namespace mvk
{
    // -------------------------------------------------------------------------
    // Data structures and type definitions
    // -------------------------------------------------------------------------
    
    // Uniform buffer for matrices (matches gl_render.h)
    struct ubo_matrix
    {
        mat4 mvp;
        mat4 model;
        mat4 view;
        vec4 view_pos;
    };

    // Light structure (matches gl_render.h)
    typedef struct
    {
        vec4 position;
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;
    } ubo_light;

    // Material structure (matches gl_render.h)
    typedef struct
    {
        vec4 ambient;
        vec4 diffuse;
        vec4 specular;
        float shininess;
        float padding[3]; // Padding to ensure 16-byte alignment
    } ubo_material;

    // Line structure for line rendering
    typedef struct
    {
        vec3 start;
        vec3 end;
        vec3 color;
        float thickness;
        VkCompareOp depth_func; // Metal/Vulkan depth function
    } Line;

    // Mesh structure for rendering
    struct vk_mesh
    {
        VkBuffer vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
        VkBuffer indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
        VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        
        unsigned int mesh_vertex_count = 0;
        unsigned int mesh_index_count = 0;
        bool auto_draw = true;
        mat4 model_matrix = {};
    };
    
    // Validation layers for debug mode
    std::vector<const char*> validationLayers = {
#ifdef _DEBUG
        "VK_LAYER_KHRONOS_validation"
#endif
    };

    // Required device extensions
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    // Forward declarations for functions
    void draw_line_ex(float thickness, vec3 start, vec3 end, vec3 color, VkCompareOp depth_func);
    
    // -------------------------------------------------------------------------
    // Global variables
    // -------------------------------------------------------------------------
    
    // Vulkan handles
    static VkInstance instance = VK_NULL_HANDLE;
    static VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    static VkDevice device = VK_NULL_HANDLE;
    static VkQueue graphicsQueue = VK_NULL_HANDLE;
    static VkQueue presentQueue = VK_NULL_HANDLE;
    static VkSurfaceKHR surface = VK_NULL_HANDLE;
    static VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    static std::vector<VkImage> swapChainImages;
    static std::vector<VkImageView> swapChainImageViews;
    static VkFormat swapChainImageFormat;
    static VkExtent2D swapChainExtent;
    static VkRenderPass renderPass = VK_NULL_HANDLE;
    static std::vector<VkFramebuffer> swapChainFramebuffers;
    static VkCommandPool commandPool = VK_NULL_HANDLE;
    static std::vector<VkCommandBuffer> commandBuffers;
    static VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    static VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    static VkFence inFlightFence = VK_NULL_HANDLE;
    
    // Pipeline resources
    static VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    static VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    static VkPipelineLayout instancedPipelineLayout = VK_NULL_HANDLE;
    static VkPipeline instancedPipeline = VK_NULL_HANDLE;
    static VkPipelineLayout linePipelineLayout = VK_NULL_HANDLE;
    static VkPipeline linePipeline = VK_NULL_HANDLE;
    
    // Descriptor resources
    static VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    static VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    
    // Depth resources
    static VkImage depthImage = VK_NULL_HANDLE;
    static VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    static VkImageView depthImageView = VK_NULL_HANDLE;
    
    // Mesh and material resources
    static std::vector<vk_mesh> g_meshes;
    static VkBuffer materialBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory materialBufferMemory = VK_NULL_HANDLE;
    static VkBuffer lightBuffer = VK_NULL_HANDLE;
    static VkDeviceMemory lightBufferMemory = VK_NULL_HANDLE;
    
    // Current light and material data
    static ubo_light g_currentLight = {
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.2f, 0.0f, 0.2f, 1.0f},
        {0.8f, 0.3f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f}
    };

    static ubo_material g_currentMaterial = {
        {0.1f, 0.1f, 0.1f, 1.0f},
        {0.8f, 0.8f, 0.8f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        128.0f
    };
    
    // Line rendering resources
    static struct {
        VkBuffer vertexBuffer;
        VkDeviceMemory vertexBufferMemory;
        int max_lines;
        int count;
        Line* lines;
        vertex* vertices;
        mat4 view_proj;
    } g_lines = {0};

    // Global variables
    static u32 g_width = 800;
    static u32 g_height = 600;
    static void* mappedMemory = nullptr;  // For MVP uniform buffer mapping

    // -------------------------------------------------------------------------
    // Helper functions (declarations)
    // -------------------------------------------------------------------------
    
    // Vulkan initialization helpers
    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
                                               const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, 
                                             const VkAllocationCallbacks* pAllocator);
    static bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers);
    static bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions);
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static VkShaderModule createShaderModule(const char* code, size_t codeSize);
    
    // Buffer helpers
    static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                           VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                           VkDeviceMemory& bufferMemory);
    static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    static VkFormat findDepthFormat();

    // Add these structures before initialization functions

    // Structure to store queue family indices
    struct QueueFamilyIndices {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        
        bool isComplete() {
            return graphicsFamilyHasValue && presentFamilyHasValue;
        }
    };

    // Structure to store swap chain support details
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    // Helper function declarations
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    int line_render_init(int max_lines);

    // -------------------------------------------------------------------------
    // Helper functions (implementations)
    // -------------------------------------------------------------------------
    
    // Create a Vulkan buffer
    static void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, 
                           VkMemoryPropertyFlags properties, VkBuffer& buffer, 
                           VkDeviceMemory& bufferMemory)
    {
        // Create the buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create buffer!\n");
            return;
        }

        // Get memory requirements for the buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        // Allocate memory for the buffer
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            fprintf(stderr, "Failed to allocate buffer memory!\n");
            return;
        }

        // Bind memory to the buffer
        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    // Copy data between Vulkan buffers
    static void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        // Create a temporary command buffer for the data transfer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer tempCommandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &tempCommandBuffer);

        // Start recording the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(tempCommandBuffer, &beginInfo);

        // Copy data from source to destination buffer
        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(tempCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        // End recording and submit the command buffer
        vkEndCommandBuffer(tempCommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &tempCommandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);  // Wait for the copy to complete

        // Clean up the temporary command buffer
        vkFreeCommandBuffers(device, commandPool, 1, &tempCommandBuffer);
    }

    // Find a memory type that matches requirements
    static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && 
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        fprintf(stderr, "Failed to find suitable memory type!\n");
        return 0;
    }

    // -------------------------------------------------------------------------
    // Initialization helper functions (implementations)
    // -------------------------------------------------------------------------
    
    // Create the Vulkan instance with required extensions for macOS/MoltenVK
    static VkResult CreateInstance() {
        // Check for validation layer support if in debug mode
        std::vector<const char*> validationLayers;
#ifdef _DEBUG
        validationLayers.push_back("VK_LAYER_KHRONOS_validation");
        if (!checkValidationLayerSupport(validationLayers)) {
            fprintf(stderr, "Validation layers requested, but not available!\n");
        }
#endif

        // Application info
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "BoidNode MoltenVK Renderer";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "BoidNode";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        
        // Required extensions for macOS/MoltenVK
        std::vector<const char*> extensions = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef __APPLE__
            VK_MVK_MACOS_SURFACE_EXTENSION_NAME,  // macOS specific
#endif
#ifdef _DEBUG
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
        };
        
        // Create instance
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        
#ifdef _DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        
        // Debug messenger for validation layers
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        // Setup debug messenger here...
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
#else
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
#endif
        
        // Create the instance
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create Vulkan instance! Error code: %d\n", result);
            return result;
        }
        
        printf("Vulkan instance created successfully\n");
        return VK_SUCCESS;
    }

    // Create a window surface for macOS
    static VkResult CreateMacOSSurface(void* windowHandle) {
#ifdef __APPLE__
        // On macOS, we need to create a CAMetalLayer-backed view
        // For this implementation, we assume NSView* is passed as windowHandle
        
        // Use the Metal extension to create a surface from the NSView
        VkMacOSSurfaceCreateInfoMVK surfaceInfo;
        surfaceInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        surfaceInfo.pNext = NULL;
        surfaceInfo.flags = 0;
        surfaceInfo.pView = windowHandle;
        
        VkResult result = vkCreateMacOSSurfaceMVK(instance, &surfaceInfo, NULL, &surface);
        if (result != VK_SUCCESS) {
            fprintf(stderr, "Failed to create macOS surface! Error code: %d\n", result);
            return result;
        }
        
        printf("macOS surface created successfully\n");
        return VK_SUCCESS;
#else
        fprintf(stderr, "Attempted to create macOS surface on non-Apple platform\n");
        return VK_ERROR_INITIALIZATION_FAILED;
#endif
    }

    // Create a shader module from compiled SPIR-V code
    static VkShaderModule createShaderModule(const char* code, size_t codeSize) {
        VkShaderModuleCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.pNext = NULL;
        createInfo.flags = 0;
        createInfo.codeSize = codeSize;
        createInfo.pCode = (const uint32_t*)code;
        
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create shader module!\n");
            return VK_NULL_HANDLE;
        }
        
        return shaderModule;
    }

    // Create the descriptor set layout for uniform buffers
    static void createDescriptorSetLayout() {
        // Binding for MVP matrix buffer
        VkDescriptorSetLayoutBinding mvpLayoutBinding{};
        mvpLayoutBinding.binding = 0;
        mvpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        mvpLayoutBinding.descriptorCount = 1;
        mvpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        
        // Binding for material properties
        VkDescriptorSetLayoutBinding materialLayoutBinding{};
        materialLayoutBinding.binding = 1;
        materialLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        materialLayoutBinding.descriptorCount = 1;
        materialLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        // Binding for light properties
        VkDescriptorSetLayoutBinding lightLayoutBinding{};
        lightLayoutBinding.binding = 2;
        lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lightLayoutBinding.descriptorCount = 1;
        lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        std::array<VkDescriptorSetLayoutBinding, 3> bindings = {
            mvpLayoutBinding, materialLayoutBinding, lightLayoutBinding
        };
        
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create descriptor set layout!\n");
        }
    }

    // Create graphics pipeline with vertex and fragment shaders
    static void createGraphicsPipeline() {
        uint32_t vertex_shader_size = 0;
        uint32_t* vert_shader_code = read_file("shaders/basic_vertex.spv", &vertex_shader_size);
        uint32_t fragment_shader_size = 0;
        uint32_t* frag_shader_code = read_file("shaders/basic_fragment.spv", &fragment_shader_size);
        
        // Check if shaders were loaded successfully
        if (!vert_shader_code || !frag_shader_code) {
            fprintf(stderr, "Failed to load shader files\n");
            exit(-1);
        }
        
        VkShaderModule vertShaderModule = createShaderModule((const char*)vert_shader_code, vertex_shader_size);
        VkShaderModule fragShaderModule = createShaderModule((const char*)frag_shader_code, fragment_shader_size);
        
        if (vertShaderModule == VK_NULL_HANDLE || fragShaderModule == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create shader modules!\n");
            return;
        }
        
        // Vertex shader stage
        VkPipelineShaderStageCreateInfo vertShaderStageInfo;
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.pNext = NULL;
        vertShaderStageInfo.flags = 0;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        vertShaderStageInfo.pSpecializationInfo = NULL;
        
        // Fragment shader stage
        VkPipelineShaderStageCreateInfo fragShaderStageInfo;
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.pNext = NULL;
        fragShaderStageInfo.flags = 0;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        fragShaderStageInfo.pSpecializationInfo = NULL;
        
        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Vertex input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        
        // Define vertex attribute description (matches our vertex struct)
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        // Attributes: position, normal, texcoord
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        
        // Position attribute (location = 0)
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(vertex, position);
        
        // Normal attribute (location = 1)
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(vertex, normal);
        
        // Texcoord attribute (location = 2)
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(vertex, texcoord);
        
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
        
        // Input assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;
        
        // Viewport and scissor
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapChainExtent.width;
        viewport.height = (float)swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
        
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;
        
        // Rasterizer
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        
        // Multisampling
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        
        // Depth stencil
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.stencilTestEnable = VK_FALSE;
        
        // Color blending
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        
        // Pipeline layout (for uniform variables)
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create pipeline layout!\n");
            return;
        }
        
        // Create the graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create graphics pipeline!\n");
            return;
        }
        
        // Clean up shader modules
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        
        printf("Graphics pipeline created successfully\n");
    }

    // Create descriptor pool for descriptor sets
    static void createDescriptorPool() {
        // We need 3 types of descriptors: uniform buffers for MVP, materials, and lights
        std::array<VkDescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 100; // Maximum number of uniform buffer descriptors
        
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 100; // Maximum number of descriptor sets
        
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create descriptor pool!\n");
        }
    }

    // Create the render pass
    static void createRenderPass() {
        // Color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = findDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Color attachment reference
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Depth attachment reference
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Define the subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // Subpass dependencies for layout transitions
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        // Create the array of attachments
        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        // Create the render pass
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create render pass!\n");
        } else {
            printf("Render pass created successfully\n");
        }
    }

    // Find a suitable depth format
    static VkFormat findDepthFormat() {
        // Check for the most common depth formats in order of preference
        std::vector<VkFormat> candidates = {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            // We need the format to be supported as a depth/stencil attachment
            if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                return format;
            }
        }

        fprintf(stderr, "Failed to find suitable depth format!\n");
        return VK_FORMAT_D32_SFLOAT; // Default fallback
    }

    // Find queue families that support graphics and presentation
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;
        
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        
        // Find a queue family that supports graphics commands
        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                indices.graphicsFamilyHasValue = true;
            }
            
            // Check if this queue family supports presentation
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                indices.presentFamily = i;
                indices.presentFamilyHasValue = true;
            }
            
            // If we've found both, we can stop searching
            if (indices.isComplete()) {
                break;
            }
        }
        
        return indices;
    }

    // Select a physical device (GPU)
    static void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        if (deviceCount == 0) {
            fprintf(stderr, "Failed to find GPUs with Vulkan support!\n");
            return;
        }
        
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        
        printf("Found %d physical devices\n", deviceCount);
        
        // Check if at least one device meets our requirements
        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
            
            printf("Checking device: %s\n", deviceProperties.deviceName);
            
            // Check if the device supports the extensions we need (e.g. swapchain)
            std::vector<const char*> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };
            
            bool extensionsSupported = checkDeviceExtensionSupport(device, deviceExtensions);
            
            // Check if the device has queue families that support our needs
            QueueFamilyIndices indices = findQueueFamilies(device);
            
            // Check for adequate swap chain support
            bool swapChainAdequate = false;
            if (extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }
            
            // Is this device suitable?
            if (indices.isComplete() && extensionsSupported && swapChainAdequate) {
                physicalDevice = device;
                printf("Selected physical device: %s\n", deviceProperties.deviceName);
                break;
            }
        }
        
        if (physicalDevice == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to find a suitable GPU!\n");
        }
    }

    // Create logical device and queues
    static void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            indices.graphicsFamily,
            indices.presentFamily
        };
        
        // We need to create a queue for each unique queue family we'll be using
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        // Specify the device features we need
        
        // Enable device features we need (most basic 3D features are enabled by default)
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.fillModeNonSolid = VK_TRUE; // Needed for wireframe rendering
        
        // Create the logical device
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        
        // Enable device extensions (swapchain is required)
        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        
        // Legacy compatibility with older implementations
        #ifdef _DEBUG
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
        #else
        createInfo.enabledLayerCount = 0;
        #endif
        
        // Create the logical device
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create logical device!\n");
            return;
        }
        
        // Get handles to the queues we need
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        
        // If presentation queue is different, get a handle to it as well
        if (indices.graphicsFamily != indices.presentFamily) {
            vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
        } else {
            presentQueue = graphicsQueue;
        }
        
        printf("Logical device and queues created successfully\n");
    }

    // Query swap chain support details for a given physical device
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        memset(&details, 0, sizeof(SwapChainSupportDetails));
        
        // Get the basic surface capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
        
        // Get the supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
        
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
        }
        
        // Get the supported presentation modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);
        
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
        }
        
        return details;
    }

    // Choose the best surface format from available options
    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // Prefer SRGB if available (better color accuracy)
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && 
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        
        // If preferred format is not available, just take the first one
        return availableFormats[0];
    }

    // Choose the best presentation mode (vsync, mailbox, etc)
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        // Mailbox mode is preferred if available (triple buffering)
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        
        // FIFO (vsync) is guaranteed to be available
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    // Choose the swap chain's extent (resolution)
    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != UINT32_MAX) {
            // If the surface size is defined, we must use it
            return capabilities.currentExtent;
        } else {
            // Otherwise, we can choose within the min/max bounds
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(g_width),
                static_cast<uint32_t>(g_height)
            };
            
            // Clamp to the allowed bounds
            actualExtent.width = std::max(
                capabilities.minImageExtent.width,
                std::min(capabilities.maxImageExtent.width, actualExtent.width));
            actualExtent.height = std::max(
                capabilities.minImageExtent.height,
                std::min(capabilities.maxImageExtent.height, actualExtent.height));
                
            return actualExtent;
        }
    }

    // Create the swap chain
    static void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
        
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);
        
        // Determine how many images to use in the swap chain
        // Request at least one more than the minimum to avoid waiting on the driver
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        
        // Make sure we don't exceed the maximum (0 means no maximum)
        if (swapChainSupport.capabilities.maxImageCount > 0 && 
            imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }
        
        // Create the swap chain
        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // Always 1 unless developing a stereoscopic 3D app
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        // Set up queue family indices for the swap chain
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};
        
        // Handle case where graphics and presentation queues differ
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }
        
        // Don't apply any pre-transformation
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        
        // Don't blend with other windows
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        
        // Set the presentation mode
        createInfo.presentMode = presentMode;
        
        // We don't care about the color of obscured pixels
        createInfo.clipped = VK_TRUE;
        
        // For now, we don't have a previous swapchain to reference
        createInfo.oldSwapchain = VK_NULL_HANDLE;
        
        // Create the swap chain
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create swap chain!\n");
            return;
        }
        
        // Get the swap chain images
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        
        // Store the format and extent for later use
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;
        
        printf("Swap chain created successfully with %d images\n", imageCount);
    }

    // Check if a physical device supports the required extensions
    static bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions) {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        
        // Create a set of all required extensions
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        
        // Remove each available extension from the set of required extensions
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }
        
        // If the set is empty, all required extensions are supported
        return requiredExtensions.empty();
    }

    // Create image views for the swap chain images
    static void createImageViews() {
        swapChainImageViews.resize(swapChainImages.size());
        
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages[i];
            
            // Specify how to interpret the image data
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat;
            
            // Default color mapping
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            
            // Define the image's purpose and which part to access
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            
            // Create the image view
            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create image views!");
                return;
            }
        }
        
        printf("Created %zu swap chain image views\n", swapChainImageViews.size());
    }

    // Create a depth image for depth testing
    static void createDepthResources() {
        VkFormat depthFormat = findDepthFormat();
        
        // Create the depth image
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapChainExtent.width;
        imageInfo.extent.height = swapChainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        if (vkCreateImage(device, &imageInfo, nullptr, &depthImage) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create depth image!\n");
            return;
        }
        
        // Allocate memory for the depth image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, depthImage, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        
        if (vkAllocateMemory(device, &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS) {
            fprintf(stderr, "Failed to allocate depth image memory!\n");
            return;
        }
        
        vkBindImageMemory(device, depthImage, depthImageMemory, 0);
        
        // Create the depth image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = depthImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = depthFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        
        if (vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create depth image view!\n");
            return;
        }
        
        // Transition the depth image layout for optimal use
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = depthImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
        
        endSingleTimeCommands(commandBuffer);
        
        printf("Depth resources created successfully\n");
    }
    
    // Helper function to execute a one-time command
    VkCommandBuffer beginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo;
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.pNext = NULL;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
        
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
        
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = NULL;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        
        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        
        return commandBuffer;
    }
    
    // Submit and cleanup the temporary command buffer
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);
        
        VkSubmitInfo submitInfo;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = NULL;
        submitInfo.waitSemaphoreCount = 0;
        submitInfo.pWaitSemaphores = NULL;
        submitInfo.pWaitDstStageMask = NULL;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 0;
        submitInfo.pSignalSemaphores = NULL;
        
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);
        
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }
    
    // Create framebuffers for the swap chain images
    static void createFramebuffers() {
        swapChainFramebuffers.resize(swapChainImageViews.size());
        
        for (size_t i = 0; i < swapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = {
                swapChainImageViews[i],
                depthImageView
            };
            
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = swapChainExtent.width;
            framebufferInfo.height = swapChainExtent.height;
            framebufferInfo.layers = 1;
            
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
                fprintf(stderr, "Failed to create framebuffer %zu!\n", i);
            }
        }
        
        printf("Created %zu framebuffers\n", swapChainFramebuffers.size());
    }
    
    // Create command pool and allocate command buffers
    static void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
        
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create command pool!\n");
            return;
        }
        
        printf("Command pool created successfully\n");
    }
    
    // Allocate command buffers from the command pool
    static void createCommandBuffers() {
        commandBuffers.resize(swapChainFramebuffers.size());
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
        
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            fprintf(stderr, "Failed to allocate command buffers!\n");
            return;
        }
        
        printf("Allocated %zu command buffers\n", commandBuffers.size());
    }
    
    // Create synchronization objects (semaphores and fences)
    static void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so we don't wait indefinitely on the first frame
        
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create synchronization objects!\n");
            return;
        }
        
        printf("Synchronization objects created successfully\n");
    }
    
    // Check if validation layers are supported
    static bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers) {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        
        // Check if all requested layers are available
        for (const char* layerName : validationLayers) {
            bool layerFound = false;
            
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }
            
            if (!layerFound) {
                return false;
            }
        }
        
        return true;
    }

    // -------------------------------------------------------------------------
    // Public API (implementations)
    // -------------------------------------------------------------------------

    // Initialize the MoltenVK renderer
    int init(void* windowHandle, int width, int height)
    {
        // Store window dimensions for later use
        g_width = width;
        g_height = height;
        
        printf("MoltenVK init: Starting initialization with window size %dx%d\n", width, height);
        
        // Step 1: Create Vulkan instance with needed extensions for macOS/MoltenVK
        if (CreateInstance() != VK_SUCCESS) {
            fprintf(stderr, "Failed to create Vulkan instance\n");
            return -1;
        }
        
        printf("MoltenVK init: Created Vulkan instance\n");
        
        // Step 2: Create macOS surface
        if (CreateMacOSSurface(windowHandle) != VK_SUCCESS) {
            fprintf(stderr, "Failed to create macOS surface\n");
            return -1;
        }
        
        printf("MoltenVK init: Created macOS surface\n");
        
        // Step 3: Select physical device
        pickPhysicalDevice();
        if (physicalDevice == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to find a suitable GPU\n");
            return -1;
        }
        
        printf("MoltenVK init: Selected physical device\n");
        
        // Step 4: Create logical device and queues
        createLogicalDevice();
        if (device == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create logical device\n");
            return -1;
        }
        
        printf("MoltenVK init: Created logical device\n");
        
        // Step 5: Create command pool (needs to happen before swap chain)
        createCommandPool();
        printf("MoltenVK init: Created command pool\n");
        
        // Step 6: Create swap chain
        createSwapChain();
        if (swapChain == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create swap chain\n");
            return -1;
        }
        
        printf("MoltenVK init: Created swap chain\n");
        
        // Step 7: Create image views
        createImageViews();
        
        // Step 8: Create render pass
        createRenderPass();
        if (renderPass == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create render pass\n");
            return -1;
        }
        
        // Step 9: Create descriptor set layout
        createDescriptorSetLayout();
        if (descriptorSetLayout == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create descriptor set layout\n");
            return -1;
        }
        
        // Step 10: Create descriptor pool
        createDescriptorPool();
        if (descriptorPool == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create descriptor pool\n");
            return -1;
        }
        
        // Step 11: Create depth resources
        createDepthResources();
        if (depthImage == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create depth resources\n");
            return -1;
        }
        
        // Step 12: Create framebuffers
        createFramebuffers();
        
        // Step 13: Create graphics pipeline
        createGraphicsPipeline();
        if (graphicsPipeline == VK_NULL_HANDLE) {
            fprintf(stderr, "Failed to create graphics pipeline\n");
            return -1;
        }
        
        // Step 14: Create command buffers
        createCommandBuffers();
        
        // Step 15: Create synchronization objects
        createSyncObjects();
        
        // Step 16: Create material and light buffers
        createBuffer(sizeof(ubo_material), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   materialBuffer, materialBufferMemory);
        
        createBuffer(sizeof(ubo_light), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   lightBuffer, lightBufferMemory);
        
        // Initialize material and light data
        void* data;
        vkMapMemory(device, materialBufferMemory, 0, sizeof(ubo_material), 0, &data);
        memcpy(data, &g_currentMaterial, sizeof(ubo_material));
        vkUnmapMemory(device, materialBufferMemory);
        
        vkMapMemory(device, lightBufferMemory, 0, sizeof(ubo_light), 0, &data);
        memcpy(data, &g_currentLight, sizeof(ubo_light));
        vkUnmapMemory(device, lightBufferMemory);
        
        // Step 17: Initialize line rendering
        int lineResult = line_render_init(1000); // Allow up to 1000 lines
        if (lineResult != 0) {
            fprintf(stderr, "Failed to initialize line rendering\n");
            return -1;
        }
        
        printf("MoltenVK renderer initialized successfully with Vulkan API\n");
        return 0; // Success
    }
    
    // Set light properties
    void set_light(vec3 ambient, vec3 diffuse, vec3 specular, vec3 position)
    {
        g_currentLight.position = {position.x, position.y, position.z, 1.0f};
        g_currentLight.ambient = {ambient.x, ambient.y, ambient.z, 1.0f};
        g_currentLight.diffuse = {diffuse.x, diffuse.y, diffuse.z, 1.0f};
        g_currentLight.specular = {specular.x, specular.y, specular.z, 1.0f};
        
        // Update light buffer
        void* data;
        vkMapMemory(device, lightBufferMemory, 0, sizeof(ubo_light), 0, &data);
        memcpy(data, &g_currentLight, sizeof(ubo_light));
        vkUnmapMemory(device, lightBufferMemory);
    }
    
    // Set material properties
    void set_material(const ubo_material* material)
    {
        if (material) {
            memcpy(&g_currentMaterial, material, sizeof(ubo_material));
            
            // Update material buffer
            void* data;
            vkMapMemory(device, materialBufferMemory, 0, sizeof(ubo_material), 0, &data);
            memcpy(data, &g_currentMaterial, sizeof(ubo_material));
            vkUnmapMemory(device, materialBufferMemory);
        }
    }
    
    // Add a mesh to be rendered
    vk_mesh* add_mesh(const Mesh* mesh, bool auto_draw)
    {
        if (!mesh || !mesh->vertices || mesh->vertexCount == 0) {
            fprintf(stderr, "Invalid mesh data.\n");
            return nullptr;
        }
        
        vk_mesh render_mesh = {};
        render_mesh.auto_draw = auto_draw;
        render_mesh.mesh_vertex_count = mesh->vertexCount;
        render_mesh.mesh_index_count = mesh->indexCount;
        render_mesh.model_matrix = matrix4::identity();
        
        // Create vertex buffer
        VkDeviceSize bufferSize = sizeof(vertex) * mesh->vertexCount;
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, mesh->vertices, bufferSize);
        vkUnmapMemory(device, stagingBufferMemory);
        
        createBuffer(bufferSize, 
                   VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                   render_mesh.vertexBuffer, render_mesh.vertexBufferMemory);
        
        copyBuffer(stagingBuffer, render_mesh.vertexBuffer, bufferSize);
        
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        
        // Create index buffer if needed
        if (mesh->indices && mesh->indexCount > 0) {
            VkDeviceSize indexBufferSize = sizeof(uint32_t) * mesh->indexCount;
            
            VkBuffer indexStagingBuffer;
            VkDeviceMemory indexStagingBufferMemory;
            createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       indexStagingBuffer, indexStagingBufferMemory);
            
            void* indexData;
            vkMapMemory(device, indexStagingBufferMemory, 0, indexBufferSize, 0, &indexData);
            memcpy(indexData, mesh->indices, indexBufferSize);
            vkUnmapMemory(device, indexStagingBufferMemory);
            
            createBuffer(indexBufferSize,
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                       render_mesh.indexBuffer, render_mesh.indexBufferMemory);
            
            copyBuffer(indexStagingBuffer, render_mesh.indexBuffer, indexBufferSize);
            
            vkDestroyBuffer(device, indexStagingBuffer, nullptr);
            vkFreeMemory(device, indexStagingBufferMemory, nullptr);
        }
        
        // Create uniform buffer for model matrices
        createBuffer(sizeof(ubo_matrix), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   render_mesh.uniformBuffer, render_mesh.uniformBufferMemory);
        
        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        
        if (vkAllocateDescriptorSets(device, &allocInfo, &render_mesh.descriptorSet) != VK_SUCCESS) {
            fprintf(stderr, "Failed to allocate descriptor set!\n");
            return nullptr;
        }
        
        // Update descriptor set
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = render_mesh.uniformBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(ubo_matrix);
        
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = render_mesh.descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        
        g_meshes.push_back(render_mesh);
        return &g_meshes[g_meshes.size() - 1];
    }
    
    // Set the MVP matrix for rendering
    void set_mvp(const mat4 view, const mat4 projection, const camera cam)
    {
        mat4 vp = matrix4::mat4_mult(projection, view);
        
        for (size_t i = 0; i < g_meshes.size(); i++) {
            ubo_matrix temp{};
            mat4 mvp = matrix4::mat4_mult(vp, g_meshes[i].model_matrix);
            
            memcpy(temp.mvp.m, mvp.m, sizeof(mat4));
            memcpy(temp.view.m, view.m, sizeof(mat4));
            memcpy(temp.model.m, g_meshes[i].model_matrix.m, sizeof(mat4));
            
            memcpy(&temp.view_pos, &cam.position, sizeof(vec3));
            temp.view_pos.w = 1.0f;
            
            void* data;
            vkMapMemory(device, g_meshes[i].uniformBufferMemory, 0, sizeof(ubo_matrix), 0, &data);
            memcpy(data, &temp, sizeof(ubo_matrix));
            vkUnmapMemory(device, g_meshes[i].uniformBufferMemory);
        }
        
        g_lines.view_proj = matrix4::mat4_mult(projection, view);
    }
    
    // Draw a line with the standard depth function
    void draw_line(float thickness, vec3 start, vec3 end, vec3 color)
    {
        draw_line_ex(thickness, start, end, color, VK_COMPARE_OP_LESS);
    }
    
    // Draw a line with a custom depth function
    void draw_line_ex(float thickness, vec3 start, vec3 end, vec3 color, VkCompareOp depth_func)
    {
        if (g_lines.count >= g_lines.max_lines) {
            fprintf(stderr, "Line render: Max lines exceeded\n");
            return;
        }
        
        g_lines.lines[g_lines.count] = {};
        g_lines.lines[g_lines.count].start = start;
        g_lines.lines[g_lines.count].end = end;
        g_lines.lines[g_lines.count].color = color;
        g_lines.lines[g_lines.count].thickness = thickness;
        g_lines.lines[g_lines.count].depth_func = depth_func;
        g_lines.count++;
    }
    
    // Initialize line rendering
    int line_render_init(int max_lines)
    {
        if (max_lines <= 0) {
            fprintf(stderr, "Line render: Invalid max lines\n");
            return -1;
        }
        
        g_lines.max_lines = max_lines;
        g_lines.lines = (Line*)malloc(sizeof(Line) * max_lines);
        g_lines.vertices = (vertex*)malloc(sizeof(vertex) * max_lines * 6);
        
        if (!g_lines.lines || !g_lines.vertices) {
            fprintf(stderr, "Line render: Memory allocation failed\n");
            return -1;
        }
        
        // Create vertex buffer for lines
        VkDeviceSize bufferSize = sizeof(vertex) * max_lines * 6;
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   g_lines.vertexBuffer, g_lines.vertexBufferMemory);
        
        return 0;
    }
    
    // Generate vertices for lines
    static void generate_line_vertices()
    {
        // Similar implementation as in gl_render.h, but adapted for Vulkan
        // This would transform lines to screen space and generate quads
        
        int v_idx = 0;
        float fw = (float)swapChainExtent.width;
        float fh = (float)swapChainExtent.height;
        
        // Process each line and convert to screen-space quads
        for (int i = 0; i < g_lines.count; i++) {
            Line line = g_lines.lines[i];
            
            // Transform start and end points to clip space
            vec4 clip_start = matrix4::mat4_mult_vec4(g_lines.view_proj,
                                                   vec4(line.start.x, line.start.y, line.start.z, 1.0f));
            vec4 clip_end = matrix4::mat4_mult_vec4(g_lines.view_proj,
                                                 vec4(line.end.x, line.end.y, line.end.z, 1.0f));
            
            // Perspective divide to get NDC coordinates
            vec3 ndc_start = vec3(clip_start.x / clip_start.w,
                               clip_start.y / clip_start.w,
                               clip_start.z / clip_start.w);
            vec3 ndc_end = vec3(clip_end.x / clip_end.w,
                             clip_end.y / clip_end.w,
                             clip_end.z / clip_end.w);
            
            // Convert NDC to screen space coordinates
            vec2 screen_start = vec2((ndc_start.x * 0.5f + 0.5f) * fw,
                                  (ndc_start.y * 0.5f + 0.5f) * fh);
            vec2 screen_end = vec2((ndc_end.x * 0.5f + 0.5f) * fw,
                                (ndc_end.y * 0.5f + 0.5f) * fh);
            
            // Compute the screen-space direction and perpendicular vector
            vec2 screen_dir = screen_end - screen_start;
            float len = sqrtf(screen_dir.x * screen_dir.x + screen_dir.y * screen_dir.y);
            if (len < 0.0001f)
                continue;
            vec2 screen_perp = vec2(-screen_dir.y, screen_dir.x) / len;
            
            // Apply the line thickness (in pixels) as an offset in screen space
            vec2 offset = screen_perp * line.thickness;
            
            // Compute four corner positions in screen space
            vec2 s0 = screen_start + offset;
            vec2 s1 = screen_start - offset;
            vec2 s2 = screen_end + offset;
            vec2 s3 = screen_end - offset;
            
            // Convert these back to NDC space
            vec2 ndc0 = vec2((s0.x / fw) * 2.0f - 1.0f, (s0.y / fh) * 2.0f - 1.0f);
            vec2 ndc1 = vec2((s1.x / fw) * 2.0f - 1.0f, (s1.y / fh) * 2.0f - 1.0f);
            vec2 ndc2 = vec2((s2.x / fw) * 2.0f - 1.0f, (s2.y / fh) * 2.0f - 1.0f);
            vec2 ndc3 = vec2((s3.x / fw) * 2.0f - 1.0f, (s3.y / fh) * 2.0f - 1.0f);
            
            // Reconstruct clip space positions using original depth and w values
            vec4 clip0 = vec4(ndc0.x * clip_start.w, ndc0.y * clip_start.w, ndc_start.z * clip_start.w, clip_start.w);
            vec4 clip1 = vec4(ndc1.x * clip_start.w, ndc1.y * clip_start.w, ndc_start.z * clip_start.w, clip_start.w);
            vec4 clip2 = vec4(ndc2.x * clip_end.w, ndc2.y * clip_end.w, ndc_end.z * clip_end.w, clip_end.w);
            vec4 clip3 = vec4(ndc3.x * clip_end.w, ndc3.y * clip_end.w, ndc_end.z * clip_end.w, clip_end.w);
            
            // Build two triangles for the line quad
            vec4 quad[6] = {
                clip0, clip1, clip2,
                clip1, clip3, clip2
            };
            
            // Store the vertices with the same color
            vec4 color = vec4(line.color.x, line.color.y, line.color.z, 1.0f);
            for (int j = 0; j < 6; j++) {
                g_lines.vertices[v_idx].position = quad[j];
                g_lines.vertices[v_idx].texcoord = color;
                v_idx++;
            }
        }
        
        // Update Vulkan buffer with the new vertices
        void* data;
        vkMapMemory(device, g_lines.vertexBufferMemory, 0, sizeof(vertex) * v_idx, 0, &data);
        memcpy(data, g_lines.vertices, sizeof(vertex) * v_idx);
        vkUnmapMemory(device, g_lines.vertexBufferMemory);
    }
    
    // Start the rendering process
    void start_draw(u32 width, u32 height)
    {
        // Wait for previous frame to finish
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);
        
        // Acquire next image from swapchain
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        
        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo);
        
        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        
        VkClearValue clearValues[2];
        clearValues[0].color = {{0.3f, 0.3f, 0.3f, 1.0f}}; // Background color
        clearValues[1].depthStencil = {1.0f, 0};
        
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues = clearValues;
        
        vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }
    
    // Render static meshes
    void draw_statics()
    {
        uint32_t imageIndex = 0; // Would be set in start_draw
        
        // Bind the graphics pipeline
        vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
        
        // Draw each mesh
        for (size_t i = 0; i < g_meshes.size(); i++) {
            vk_mesh* mesh = &g_meshes[i];
            
            if (mesh->auto_draw) {
                // Bind vertex buffer
                VkBuffer vertexBuffers[] = {mesh->vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
                
                // Bind descriptor sets (for uniform data)
                vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, 
                                      pipelineLayout, 0, 1, &mesh->descriptorSet, 0, nullptr);
                
                // Draw with or without indices
                if (mesh->indexBuffer && mesh->mesh_index_count > 0) {
                    vkCmdBindIndexBuffer(commandBuffers[imageIndex], mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(commandBuffers[imageIndex], mesh->mesh_index_count, 1, 0, 0, 0);
                } else {
                    vkCmdDraw(commandBuffers[imageIndex], mesh->mesh_vertex_count, 1, 0, 0);
                }
            }
        }
    }
    
    // Render instanced meshes
    void render_instances(vk_mesh* mesh, mat4* model_matrices, u32 count)
    {
        if (!mesh || !model_matrices || count == 0) {
            fprintf(stderr, "Invalid parameters for render_instances.\n");
            return;
        }
        
        // Would create a staging buffer for model matrices and draw instanced
        // This would involve:
        // 1. Create a buffer for the model matrices
        // 2. Copy the matrices to the buffer
        // 3. Bind the instanced pipeline
        // 4. Bind the vertex buffer
        // 5. Bind the model matrix buffer
        // 6. Draw instanced
        
        // Simplified implementation (actual implementation would be more complex)
        uint32_t imageIndex = 0; // Would be set in start_draw
        
        // Create buffer for model matrices
        VkBuffer modelBuffer;
        VkDeviceMemory modelBufferMemory;
        VkDeviceSize bufferSize = sizeof(mat4) * count;
        
        createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                   modelBuffer, modelBufferMemory);
        
        void* data;
        vkMapMemory(device, modelBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, model_matrices, bufferSize);
        vkUnmapMemory(device, modelBufferMemory);
        
        // Bind the instanced pipeline
        vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, instancedPipeline);
        
        // Bind vertex buffers (position and model matrices)
        VkBuffer vertexBuffers[] = {mesh->vertexBuffer, modelBuffer};
        VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 2, vertexBuffers, offsets);
        
        // Bind descriptor set
        vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                              instancedPipelineLayout, 0, 1, &mesh->descriptorSet, 0, nullptr);
        
        // Draw instanced with or without indices
        if (mesh->indexBuffer && mesh->mesh_index_count > 0) {
            vkCmdBindIndexBuffer(commandBuffers[imageIndex], mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(commandBuffers[imageIndex], mesh->mesh_index_count, count, 0, 0, 0);
        } else {
            vkCmdDraw(commandBuffers[imageIndex], mesh->mesh_vertex_count, count, 0, 0);
        }
        
        // Clean up
        vkDestroyBuffer(device, modelBuffer, nullptr);
        vkFreeMemory(device, modelBufferMemory, nullptr);
    }
    
    // Render lines
    void render_lines()
    {
        if (g_lines.count > 0) {
            generate_line_vertices();
            
            uint32_t imageIndex = 0; // Would be set in start_draw
            
            // Bind the line pipeline
            vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline);
            
            // Bind the vertex buffer
            VkBuffer vertexBuffers[] = {g_lines.vertexBuffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
            
            // Draw lines in batches by depth function
            VkCompareOp current_depth_func = g_lines.lines[0].depth_func;
            int batch_start = 0;
            
            for (int i = 1; i <= g_lines.count; i++) {
                if (i == g_lines.count || g_lines.lines[i].depth_func != current_depth_func) {
                    // Update depth stencil state for this batch
                    VkPipelineDepthStencilStateCreateInfo depthStencil{};
                    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
                    depthStencil.depthTestEnable = VK_TRUE;
                    depthStencil.depthWriteEnable = VK_TRUE;
                    depthStencil.depthCompareOp = current_depth_func;
                    
                    // In a complete implementation, we would use dynamic state or multiple pipelines
                    // Here we'll just assume we have a way to update the depth compare op
                    
                    // Draw batch
                    vkCmdDraw(commandBuffers[imageIndex], (i - batch_start) * 6, 1, batch_start * 6, 0);
                    
                    if (i < g_lines.count) {
                        // Start new batch
                        current_depth_func = g_lines.lines[i].depth_func;
                        batch_start = i;
                    }
                }
            }
            
            g_lines.count = 0; // Reset for next frame
        }
    }
    
    // Finish rendering and present the frame
    void end_draw()
    {
        uint32_t imageIndex = 0; // Would be set in start_draw
        
        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS) {
            fprintf(stderr, "Failed to record command buffer!\n");
            return;
        }
        
        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
            fprintf(stderr, "Failed to submit draw command buffer!\n");
            return;
        }
        
        // Present the frame
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        
        VkSwapchainKHR swapChains[] = {swapChain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        
        vkQueuePresentKHR(graphicsQueue, &presentInfo);
    }
    
    // Clean up all resources
    void cleanup()
    {
        // Wait for device to finish operations
        vkDeviceWaitIdle(device);
        
        // Clean up line rendering resources
        if (g_lines.vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, g_lines.vertexBuffer, nullptr);
            vkFreeMemory(device, g_lines.vertexBufferMemory, nullptr);
        }
        free(g_lines.lines);
        free(g_lines.vertices);
        
        // Clean up mesh resources
        for (size_t i = 0; i < g_meshes.size(); i++) {
            vk_mesh* mesh = &g_meshes[i];
            
            if (mesh->vertexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, mesh->vertexBuffer, nullptr);
                vkFreeMemory(device, mesh->vertexBufferMemory, nullptr);
            }
            
            if (mesh->indexBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, mesh->indexBuffer, nullptr);
                vkFreeMemory(device, mesh->indexBufferMemory, nullptr);
            }
            
            if (mesh->uniformBuffer != VK_NULL_HANDLE) {
                vkDestroyBuffer(device, mesh->uniformBuffer, nullptr);
                vkFreeMemory(device, mesh->uniformBufferMemory, nullptr);
            }
        }
        
        // Clean up material and light buffers
        if (materialBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, materialBuffer, nullptr);
            vkFreeMemory(device, materialBufferMemory, nullptr);
        }
        
        if (lightBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, lightBuffer, nullptr);
            vkFreeMemory(device, lightBufferMemory, nullptr);
        }
        
        // Clean up depth resources
        if (depthImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, depthImageView, nullptr);
        }
        
        if (depthImage != VK_NULL_HANDLE) {
            vkDestroyImage(device, depthImage, nullptr);
            vkFreeMemory(device, depthImageMemory, nullptr);
        }
        
        // Clean up descriptor resources
        if (descriptorPool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        }
        
        if (descriptorSetLayout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        }
        
        // Clean up pipeline resources
        if (linePipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, linePipeline, nullptr);
        }
        
        if (linePipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, linePipelineLayout, nullptr);
        }
        
        if (instancedPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, instancedPipeline, nullptr);
        }
        
        if (instancedPipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, instancedPipelineLayout, nullptr);
        }
        
        if (graphicsPipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, graphicsPipeline, nullptr);
        }
        
        if (pipelineLayout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        }
        
        // Clean up framebuffers
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }
        
        // Clean up renderpass
        if (renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device, renderPass, nullptr);
        }
        
        // Clean up command pool
        if (commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
        
        // Clean up synchronization objects
        if (imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        }
        
        if (renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        }
        
        if (inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(device, inFlightFence, nullptr);
        }
        
        // Clean up swap chain
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        
        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, swapChain, nullptr);
        }
        
        // Clean up surface
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
        }
        
        // Clean up logical device
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
        
        // Clean up instance
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
        }
    }
    


} // namespace mvk

#endif // __APPLE__
#endif // MOLTENVK_RENDER_H