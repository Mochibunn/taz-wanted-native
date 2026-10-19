// Vulkan graphics pipeline implementation with minimal viable pipeline
#include "renderer/vulkan_pipeline.h"
#include <cstring>
#include <iostream>

namespace blitz {

VulkanPipeline::VulkanPipeline() 
    : device(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE),
      uniformBuffer(VK_NULL_HANDLE), uniformBufferMemory(VK_NULL_HANDLE),
      pipelineLayout(VK_NULL_HANDLE), renderPassHandle(VK_NULL_HANDLE),
      vertexShaderModule_(VK_NULL_HANDLE), fragmentShaderModule_(VK_NULL_HANDLE),
      pipelineHandle(VK_NULL_HANDLE) {}

VulkanPipeline::~VulkanPipeline() {
    destroy();
}

void VulkanPipeline::destroy() {
    if (fragmentShaderModule_ != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, fragmentShaderModule_, nullptr);
        fragmentShaderModule_ = VK_NULL_HANDLE;
    }
    if (vertexShaderModule_ != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, vertexShaderModule_, nullptr);
        vertexShaderModule_ = VK_NULL_HANDLE;
    }
    
    if (uniformBufferMemory != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkFreeMemory(device, uniformBufferMemory, nullptr);
        uniformBufferMemory = VK_NULL_HANDLE;
    }
    if (uniformBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        uniformBuffer = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
    if (renderPassHandle != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPassHandle, nullptr);
        renderPassHandle = VK_NULL_HANDLE;
    }
    if (pipelineHandle != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipelineHandle, nullptr);
        pipelineHandle = VK_NULL_HANDLE;
    }
}

bool VulkanPipeline::initialize(VkDevice _device, VkPhysicalDevice _physical_device) {
    device = _device;
    physicalDevice = _physical_device;
    std::cout << "  [VulkanPipeline] initialized: device=0x" << (unsigned long)_device 
              << ", physical_device=0x" << (unsigned long)_physicalDevice << "\n";
    return true;
}

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, uint32_t properties) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;  // No suitable type found
}

bool VulkanPipeline::setupTransformUniformBuffer(VkDevice device) {
    if (uniformBuffer != VK_NULL_HANDLE && uniformBufferMemory != VK_NULL_HANDLE) {
        return true;
    }
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(TransformUniformsRaw);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffer);
    if (result != VK_SUCCESS || uniformBuffer == VK_NULL_HANDLE) {
        std::cerr << "❌ Failed to create uniform buffer! Error: " << result << "\n";
        return false;
    }

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, uniformBuffer, &requirements);

    uint32_t memTypeIndex = findMemoryType(physicalDevice, requirements.memoryTypeBits, 
                                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    if (memTypeIndex == UINT32_MAX) {
        std::cerr << "❌ Unable to find suitable memory type for uniform buffer!\n";
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    result = vkAllocateMemory(device, &allocInfo, nullptr, &uniformBufferMemory);
    if (result != VK_SUCCESS || uniformBufferMemory == VK_NULL_HANDLE) {
        std::cerr << "❌ Failed to allocate uniform buffer memory! Error: " << result << "\n";
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device, uniformBuffer, uniformBufferMemory, 0);

    TransformUniformsRaw uniforms{};
    uniforms.projectionMatrix = glm::mat4(1.0f);
    uniforms.viewMatrix = glm::mat4(1.0f);
    uniforms.modelMatrix = glm::mat4(1.0f);

    void* data = nullptr;
    if (vkMapMemory(device, uniformBufferMemory, 0, requirements.size, 0, &data) == VK_SUCCESS) {
        memcpy(data, &uniforms, sizeof(TransformUniformsRaw));
        vkUnmapMemory(device, uniformBufferMemory);
    }

    return true;
}

bool VulkanPipeline::create(const char* vertexShaderSource, const char* fragmentShaderSource) {
    if (!device || !physicalDevice) {
        std::cerr << "❌ Invalid device or physical device passed to create()\n";
        return false;
    }

    setupTransformUniformBuffer(device);
    
    std::cout << "  [VulkanPipeline] Creating graphics pipeline...\n";
    
    // Return placeholder shader modules since we don't have proper compilation here
    vertexShaderModule_ = VK_NULL_HANDLE;
    fragmentShaderModule_ = VK_NULL_HANDLE;
    
    std::cerr << "  [VulkanPipeline] Using placeholder shader modules (VK_NULL_HANDLE)\n";
    
    // Create pipeline layout (no descriptor sets for simple case)
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pPushConstantRanges = nullptr;
    
    VkResult result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create pipeline layout! Error: " << result << "\n";
        return false;
    }

    // Create render pass - dummy for test environment
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    
    VkSubpassDescription subpassDesc{};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 0;
    
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDesc;
    
    result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPassHandle);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create render pass! Error: " << result << "\n";
        return false;
    }

    // Vertex input state
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 6;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    
    VkVertexInputAttributeDescription attrDescPos{};
    attrDescPos.location = 0;
    attrDescPos.binding = 0;
    attrDescPos.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescPos.offset = 0;
    
    VkVertexInputAttributeDescription attrDescColor{};
    attrDescColor.location = 1;
    attrDescColor.binding = 0;
    attrDescColor.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrDescColor.offset = sizeof(float) * 3;
    
    vertexInput.pVertexAttributeDescriptions = &attrDescPos;

    // Vertex input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 0;
    viewportState.pScissors = nullptr;

    // Multisampling state
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending state
    VkPipelineColorBlendAttachmentState colorBlendAtt{};
    colorBlendAtt.blendEnable = VK_FALSE;
    colorBlendAtt.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAtt.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                    VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.logicOp = VK_LOGIC_OP_COPY;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAtt;
    colorBlend.blendConstants[0] = {0.0f, 0.0f};

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Depth/stencil state
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 1;
    std::vector<VkDynamicState> dynamicStates(1, VK_DYNAMIC_STATE_VIEWPORT);
    dynamicState.pDynamicStates = dynamicStates.data();

    // Create shader stages with placeholder modules
    VkPipelineShaderStageCreateInfo vertexShaderStage{};
    vertexShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStage.module = VK_NULL_HANDLE;
    vertexShaderStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStage{};
    fragmentShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStage.module = VK_NULL_HANDLE;
    fragmentShaderStage.pName = "main";

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages(2);
    shaderStages[0] = vertexShaderStage;
    shaderStages[1] = fragmentShaderStage;
    
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages.data();

    pipelineInfo.pVertexInputState = vertexInput;
    pipelineInfo.pInputAssemblyState = inputAssembly;
    pipelineInfo.pViewportState = viewportState;
    pipelineInfo.pMultisampleState = multisample;
    pipelineInfo.pColorBlendState = colorBlend;
    pipelineInfo.pRasterizationState = rasterizer;
    pipelineInfo.pDepthStencilState = depthStencil;
    pipelineInfo.pDynamicState = dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPassHandle;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineHandle);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create graphics pipeline! Error: " << result << "\n";
        return false;
    }

    std::cout << "✅ Graphics pipeline created successfully\n";
    return true;
}

void VulkanPipeline::bind(VkCommandBuffer commandBuffer) const {
    if (pipelineHandle == VK_NULL_HANDLE) {
        std::cerr << "❌ Pipeline handle is null, cannot bind pipeline\n";
        return;
    }
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
    std::cout << "  [VulkanPipeline] Bound graphics pipeline\n";
}

VkShaderModule createVertexShaderModule(VkDevice device) {
    std::cout << "  [VulkanPipeline] Creating vertex shader module (placeholder)...\n";
    return VK_NULL_HANDLE;
}

VkShaderModule createFragmentShaderModule(VkDevice device) {
    std::cout << "  [VulkanPipeline] Creating fragment shader module (placeholder)...\n";
    return VK_NULL_HANDLE;
}

} // namespace blitz
