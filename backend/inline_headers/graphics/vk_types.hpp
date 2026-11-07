// =========================================================================
// vk_types.hpp : the slice of the Vulkan 1.1 API the engine uses, declared by hand.
// =========================================================================
// No Vulkan header is included and nothing links against Vulkan: libvulkan is opened
// at run time and every function is looked up by name. Struct layouts and enum
// values below are copied from the Vulkan specification (vulkan_core.h).
#pragma once
#include <cstdint>
#include <cstddef>

namespace Vk {

#define VK_DEFINE_HANDLE(n) typedef struct n##_T* n;
#define VK_DEFINE_NDH(n) typedef uint64_t n;
VK_DEFINE_HANDLE(VkInstance) VK_DEFINE_HANDLE(VkPhysicalDevice) VK_DEFINE_HANDLE(VkDevice) VK_DEFINE_HANDLE(VkQueue) VK_DEFINE_HANDLE(VkCommandBuffer)
VK_DEFINE_NDH(VkBuffer) VK_DEFINE_NDH(VkImage) VK_DEFINE_NDH(VkImageView) VK_DEFINE_NDH(VkDeviceMemory) VK_DEFINE_NDH(VkRenderPass) VK_DEFINE_NDH(VkFramebuffer)
VK_DEFINE_NDH(VkPipeline) VK_DEFINE_NDH(VkPipelineLayout) VK_DEFINE_NDH(VkShaderModule) VK_DEFINE_NDH(VkCommandPool) VK_DEFINE_NDH(VkFence) VK_DEFINE_NDH(VkSampler)
VK_DEFINE_NDH(VkDescriptorSetLayout) VK_DEFINE_NDH(VkDescriptorPool) VK_DEFINE_NDH(VkDescriptorSet) VK_DEFINE_NDH(VkSemaphore) VK_DEFINE_NDH(VkSurfaceKHR) VK_DEFINE_NDH(VkSwapchainKHR)
typedef int32_t VkResult; typedef uint32_t VkFlags; typedef uint64_t VkDeviceSize; typedef uint32_t VkBool32;

enum : int32_t {
    VK_SUCCESS = 0, VK_NOT_READY = 1, VK_TIMEOUT = 2, VK_SUBOPTIMAL_KHR = 1000001003, VK_ERROR_OUT_OF_DATE_KHR = -1000001004,
    // structure types
    ST_APPLICATION_INFO = 0, ST_INSTANCE_CREATE_INFO = 1, ST_DEVICE_QUEUE_CREATE_INFO = 2, ST_DEVICE_CREATE_INFO = 3, ST_SUBMIT_INFO = 4, ST_MEMORY_ALLOCATE_INFO = 5, ST_FENCE_CREATE_INFO = 8,
    ST_BUFFER_CREATE_INFO = 12, ST_IMAGE_CREATE_INFO = 14, ST_IMAGE_VIEW_CREATE_INFO = 15, ST_SHADER_MODULE_CREATE_INFO = 16, ST_PIPELINE_SHADER_STAGE = 18, ST_PIPELINE_VERTEX_INPUT = 19,
    ST_PIPELINE_INPUT_ASSEMBLY = 20, ST_PIPELINE_VIEWPORT = 22, ST_PIPELINE_RASTERIZATION = 23, ST_PIPELINE_MULTISAMPLE = 24, ST_PIPELINE_DEPTH_STENCIL = 25, ST_PIPELINE_COLOR_BLEND = 26,
    ST_PIPELINE_DYNAMIC_STATE = 27, ST_GRAPHICS_PIPELINE_CREATE_INFO = 28, ST_PIPELINE_LAYOUT_CREATE_INFO = 30, ST_SAMPLER_CREATE_INFO = 31, ST_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32,
    ST_DESCRIPTOR_POOL_CREATE_INFO = 33, ST_DESCRIPTOR_SET_ALLOCATE_INFO = 34, ST_WRITE_DESCRIPTOR_SET = 35, ST_FRAMEBUFFER_CREATE_INFO = 37, ST_RENDER_PASS_CREATE_INFO = 38,
    ST_COMMAND_POOL_CREATE_INFO = 39, ST_COMMAND_BUFFER_ALLOCATE_INFO = 40, ST_COMMAND_BUFFER_BEGIN_INFO = 42, ST_RENDER_PASS_BEGIN_INFO = 43, ST_IMAGE_MEMORY_BARRIER = 45,
};

struct VkApplicationInfo { int32_t sType; const void* pNext; const char* pApplicationName; uint32_t applicationVersion; const char* pEngineName; uint32_t engineVersion; uint32_t apiVersion; };
struct VkInstanceCreateInfo { int32_t sType; const void* pNext; VkFlags flags; const VkApplicationInfo* pApplicationInfo; uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames; uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames; };
struct VkQueueFamilyProperties { VkFlags queueFlags; uint32_t queueCount, timestampValidBits; uint32_t minImageTransferGranularity[3]; };
struct VkDeviceQueueCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t queueFamilyIndex, queueCount; const float* pQueuePriorities; };
struct VkDeviceCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t queueCreateInfoCount; const VkDeviceQueueCreateInfo* pQueueCreateInfos; uint32_t enabledLayerCount; const char* const* ppEnabledLayerNames; uint32_t enabledExtensionCount; const char* const* ppEnabledExtensionNames; const void* pEnabledFeatures; };
struct VkMemoryType { VkFlags propertyFlags; uint32_t heapIndex; };
struct VkMemoryHeap { VkDeviceSize size; VkFlags flags; };
struct VkPhysicalDeviceMemoryProperties { uint32_t memoryTypeCount; VkMemoryType memoryTypes[32]; uint32_t memoryHeapCount; VkMemoryHeap memoryHeaps[16]; };
struct VkMemoryRequirements { VkDeviceSize size, alignment; uint32_t memoryTypeBits; };
struct VkMemoryAllocateInfo { int32_t sType; const void* pNext; VkDeviceSize allocationSize; uint32_t memoryTypeIndex; };
struct VkBufferCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkDeviceSize size; VkFlags usage; int32_t sharingMode; uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices; };
struct VkExtent3D { uint32_t width, height, depth; };
struct VkImageCreateInfo { int32_t sType; const void* pNext; VkFlags flags; int32_t imageType; int32_t format; VkExtent3D extent; uint32_t mipLevels, arrayLayers; int32_t samples, tiling; VkFlags usage; int32_t sharingMode; uint32_t queueFamilyIndexCount; const uint32_t* pQueueFamilyIndices; int32_t initialLayout; };
struct VkComponentMapping { int32_t r, g, b, a; };
struct VkImageSubresourceRange { VkFlags aspectMask; uint32_t baseMipLevel, levelCount, baseArrayLayer, layerCount; };
struct VkImageViewCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkImage image; int32_t viewType, format; VkComponentMapping components; VkImageSubresourceRange subresourceRange; };
struct VkShaderModuleCreateInfo { int32_t sType; const void* pNext; VkFlags flags; size_t codeSize; const uint32_t* pCode; };
struct VkAttachmentDescription { VkFlags flags; int32_t format, samples, loadOp, storeOp, stencilLoadOp, stencilStoreOp, initialLayout, finalLayout; };
struct VkAttachmentReference { uint32_t attachment; int32_t layout; };
struct VkSubpassDescription { VkFlags flags; int32_t pipelineBindPoint; uint32_t inputAttachmentCount; const VkAttachmentReference* pInputAttachments; uint32_t colorAttachmentCount; const VkAttachmentReference* pColorAttachments; const VkAttachmentReference* pResolveAttachments; const VkAttachmentReference* pDepthStencilAttachment; uint32_t preserveAttachmentCount; const uint32_t* pPreserveAttachments; };
struct VkSubpassDependency { uint32_t srcSubpass, dstSubpass; VkFlags srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, dependencyFlags; };
struct VkRenderPassCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t attachmentCount; const VkAttachmentDescription* pAttachments; uint32_t subpassCount; const VkSubpassDescription* pSubpasses; uint32_t dependencyCount; const VkSubpassDependency* pDependencies; };
struct VkFramebufferCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkRenderPass renderPass; uint32_t attachmentCount; const VkImageView* pAttachments; uint32_t width, height, layers; };
struct VkPushConstantRange { VkFlags stageFlags; uint32_t offset, size; };
struct VkPipelineLayoutCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t setLayoutCount; const VkDescriptorSetLayout* pSetLayouts; uint32_t pushConstantRangeCount; const VkPushConstantRange* pPushConstantRanges; };
struct VkPipelineShaderStageCreateInfo { int32_t sType; const void* pNext; VkFlags flags; int32_t stage; VkShaderModule module; const char* pName; const void* pSpecializationInfo; };
struct VkVertexInputBindingDescription { uint32_t binding, stride; int32_t inputRate; };
struct VkVertexInputAttributeDescription { uint32_t location, binding; int32_t format; uint32_t offset; };
struct VkPipelineVertexInputStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t vertexBindingDescriptionCount; const VkVertexInputBindingDescription* pVertexBindingDescriptions; uint32_t vertexAttributeDescriptionCount; const VkVertexInputAttributeDescription* pVertexAttributeDescriptions; };
struct VkPipelineInputAssemblyStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; int32_t topology; VkBool32 primitiveRestartEnable; };
struct VkViewport { float x, y, width, height, minDepth, maxDepth; };
struct VkOffset2D { int32_t x, y; }; struct VkExtent2D { uint32_t width, height; }; struct VkRect2D { VkOffset2D offset; VkExtent2D extent; };
struct VkPipelineViewportStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t viewportCount; const VkViewport* pViewports; uint32_t scissorCount; const VkRect2D* pScissors; };
struct VkPipelineRasterizationStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkBool32 depthClampEnable, rasterizerDiscardEnable; int32_t polygonMode; VkFlags cullMode; int32_t frontFace; VkBool32 depthBiasEnable; float depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor, lineWidth; };
struct VkPipelineMultisampleStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; int32_t rasterizationSamples; VkBool32 sampleShadingEnable; float minSampleShading; const uint32_t* pSampleMask; VkBool32 alphaToCoverageEnable, alphaToOneEnable; };
struct VkStencilOpState { int32_t failOp, passOp, depthFailOp, compareOp; uint32_t compareMask, writeMask, reference; };
struct VkPipelineDepthStencilStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkBool32 depthTestEnable, depthWriteEnable; int32_t depthCompareOp; VkBool32 depthBoundsTestEnable, stencilTestEnable; VkStencilOpState front, back; float minDepthBounds, maxDepthBounds; };
struct VkPipelineColorBlendAttachmentState { VkBool32 blendEnable; int32_t srcColorBlendFactor, dstColorBlendFactor, colorBlendOp, srcAlphaBlendFactor, dstAlphaBlendFactor, alphaBlendOp; VkFlags colorWriteMask; };
struct VkPipelineColorBlendStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; VkBool32 logicOpEnable; int32_t logicOp; uint32_t attachmentCount; const VkPipelineColorBlendAttachmentState* pAttachments; float blendConstants[4]; };
struct VkPipelineDynamicStateCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t dynamicStateCount; const int32_t* pDynamicStates; };
struct VkGraphicsPipelineCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t stageCount; const VkPipelineShaderStageCreateInfo* pStages; const VkPipelineVertexInputStateCreateInfo* pVertexInputState; const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState; const void* pTessellationState; const VkPipelineViewportStateCreateInfo* pViewportState; const VkPipelineRasterizationStateCreateInfo* pRasterizationState; const VkPipelineMultisampleStateCreateInfo* pMultisampleState; const VkPipelineDepthStencilStateCreateInfo* pDepthStencilState; const VkPipelineColorBlendStateCreateInfo* pColorBlendState; const VkPipelineDynamicStateCreateInfo* pDynamicState; VkPipelineLayout layout; VkRenderPass renderPass; uint32_t subpass; VkPipeline basePipelineHandle; int32_t basePipelineIndex; };
struct VkCommandPoolCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t queueFamilyIndex; };
struct VkCommandBufferAllocateInfo { int32_t sType; const void* pNext; VkCommandPool commandPool; int32_t level; uint32_t commandBufferCount; };
struct VkCommandBufferBeginInfo { int32_t sType; const void* pNext; VkFlags flags; const void* pInheritanceInfo; };
union VkClearValue { float color[4]; struct { float depth; uint32_t stencil; } ds; };
struct VkRenderPassBeginInfo { int32_t sType; const void* pNext; VkRenderPass renderPass; VkFramebuffer framebuffer; VkRect2D renderArea; uint32_t clearValueCount; const VkClearValue* pClearValues; };
struct VkSubmitInfo { int32_t sType; const void* pNext; uint32_t waitSemaphoreCount; const VkSemaphore* pWaitSemaphores; const VkFlags* pWaitDstStageMask; uint32_t commandBufferCount; const VkCommandBuffer* pCommandBuffers; uint32_t signalSemaphoreCount; const VkSemaphore* pSignalSemaphores; };
struct VkFenceCreateInfo { int32_t sType; const void* pNext; VkFlags flags; };
struct VkImageSubresourceLayers { VkFlags aspectMask; uint32_t mipLevel, baseArrayLayer, layerCount; };
struct VkOffset3D { int32_t x, y, z; };
struct VkBufferImageCopy { VkDeviceSize bufferOffset; uint32_t bufferRowLength, bufferImageHeight; VkImageSubresourceLayers imageSubresource; VkOffset3D imageOffset; VkExtent3D imageExtent; };
struct VkImageMemoryBarrier { int32_t sType; const void* pNext; VkFlags srcAccessMask, dstAccessMask; int32_t oldLayout, newLayout; uint32_t srcQueueFamilyIndex, dstQueueFamilyIndex; VkImage image; VkImageSubresourceRange subresourceRange; };
struct VkSamplerCreateInfo { int32_t sType; const void* pNext; VkFlags flags; int32_t magFilter, minFilter, mipmapMode, addressModeU, addressModeV, addressModeW; float mipLodBias; VkBool32 anisotropyEnable; float maxAnisotropy; VkBool32 compareEnable; int32_t compareOp; float minLod, maxLod; int32_t borderColor; VkBool32 unnormalizedCoordinates; };
struct VkDescriptorSetLayoutBinding { uint32_t binding; int32_t descriptorType; uint32_t descriptorCount; VkFlags stageFlags; const VkSampler* pImmutableSamplers; };
struct VkDescriptorSetLayoutCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t bindingCount; const VkDescriptorSetLayoutBinding* pBindings; };
struct VkDescriptorPoolSize { int32_t type; uint32_t descriptorCount; };
struct VkDescriptorPoolCreateInfo { int32_t sType; const void* pNext; VkFlags flags; uint32_t maxSets, poolSizeCount; const VkDescriptorPoolSize* pPoolSizes; };
struct VkDescriptorSetAllocateInfo { int32_t sType; const void* pNext; VkDescriptorPool descriptorPool; uint32_t descriptorSetCount; const VkDescriptorSetLayout* pSetLayouts; };
struct VkDescriptorImageInfo { VkSampler sampler; VkImageView imageView; int32_t imageLayout; };
struct VkWriteDescriptorSet { int32_t sType; const void* pNext; VkDescriptorSet dstSet; uint32_t dstBinding, dstArrayElement, descriptorCount; int32_t descriptorType; const VkDescriptorImageInfo* pImageInfo; const void* pBufferInfo; const void* pTexelBufferView; };

// ---- enum values used (from the specification) ----
enum : int32_t {
    FORMAT_R8G8B8A8_UNORM = 37, FORMAT_B8G8R8A8_UNORM = 44, FORMAT_R32_SFLOAT = 100, FORMAT_R32G32_SFLOAT = 103, FORMAT_R32G32B32_SFLOAT = 106, FORMAT_R32G32B32A32_SFLOAT = 109, FORMAT_D32_SFLOAT = 126,
    LAYOUT_UNDEFINED = 0, LAYOUT_GENERAL = 1, LAYOUT_COLOR_ATTACHMENT = 2, LAYOUT_DEPTH_STENCIL_ATTACHMENT = 3, LAYOUT_SHADER_READ_ONLY = 5, LAYOUT_TRANSFER_SRC = 6, LAYOUT_TRANSFER_DST = 7,
    LOAD_OP_LOAD = 0, LOAD_OP_CLEAR = 1, LOAD_OP_DONT_CARE = 2, STORE_OP_STORE = 0, STORE_OP_DONT_CARE = 1,
};
enum : VkFlags {
    BUFFER_TRANSFER_SRC = 0x1, BUFFER_TRANSFER_DST = 0x2, BUFFER_VERTEX = 0x80, IMAGE_TRANSFER_SRC = 0x1, IMAGE_TRANSFER_DST = 0x2, IMAGE_SAMPLED = 0x4, IMAGE_COLOR_ATTACHMENT = 0x10, IMAGE_DEPTH_STENCIL = 0x20,
    MEM_DEVICE_LOCAL = 0x1, MEM_HOST_VISIBLE = 0x2, MEM_HOST_COHERENT = 0x4, ASPECT_COLOR = 0x1, ASPECT_DEPTH = 0x2,
    STAGE_VERTEX = 0x1, STAGE_FRAGMENT = 0x10, STAGE_TOP_OF_PIPE = 0x1, STAGE_COLOR_OUTPUT = 0x400, STAGE_TRANSFER = 0x1000, STAGE_BOTTOM_OF_PIPE = 0x2000, STAGE_ALL_COMMANDS = 0x10000,
    ACCESS_COLOR_WRITE = 0x100, ACCESS_DEPTH_WRITE = 0x400, ACCESS_TRANSFER_READ = 0x800, ACCESS_TRANSFER_WRITE = 0x1000, ACCESS_SHADER_READ = 0x20, ACCESS_HOST_READ = 0x2000,
    QUEUE_GRAPHICS = 0x1, COMMAND_POOL_RESET_BUFFER = 0x2, CMD_ONE_TIME_SUBMIT = 0x1, COLOR_WRITE_ALL = 0xF,
};
} // namespace Vk
