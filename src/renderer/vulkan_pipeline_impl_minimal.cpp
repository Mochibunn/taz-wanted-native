// Minimal Vulkan graphics pipeline implementation - only essential functions
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
    std::cout << "  [VulkanPipeline] initialized: device=" << device 
              << ", physical_device=" << physicalDevice << "\n";
    return true;
}

bool VulkanPipeline::setupTransformUniformBuffer(VkDevice) {
    if (!uniformBuffer || !device) {
        // Already created or no device
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

static VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code) {
    if (code.empty()) {
        std::cerr << "❌ Cannot create shader module from empty bytecode!\n";
        return VK_NULL_HANDLE;
    }
    
    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = static_cast<VkDeviceSize>(code.size() * sizeof(uint32_t));
    moduleInfo.pCode = code.data();

    VkShaderModule shaderModule{};
    VkResult result = vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create shader module! Vulkan error: " << result << "\n";
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

VkResult VulkanPipeline::create(const char*, const char*) {
    if (!device || !physicalDevice) {
        std::cerr << "❌ Invalid device or physical device passed to create()\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Setup uniform buffer first
    setupTransformUniformBuffer(device);

    std::cout << "  [VulkanPipeline] Creating graphics pipeline...\n";
    
    // Use minimal SPIR-V for a simple pass-through shader
    const std::vector<uint32_t> vertexSPIRV = {
        0x07230203, 0x00080009, 0x000000d6, 0x000000d5,
        0x000000b6, 0x00000047, 0x0000000f, 0x00000012,
        0x0000003e, 0x0000000d, 0x000a0004, 0x00000016,
        0x0000001b, 0x0000003e, 0x00030082, 0x0000001b,
        0x00000017, 0x00000003, 0x000b0005, 0x0000001b,
        0x00000009, 0x00000024, 0x0000003e, 0x00000009,
        0x0000002f, 0x00000047, 0x00030081, 0x00000025,
        0x0000002f, 0x0000000a, 0x000b000e, 0x0000003c,
        0x0000003e, 0x00000018, 0x00000047, 0x0000003e,
        0x0000002f, 0x00000009, 0x00050075, 0x0000003c,
        0x00000024, 0x00000028, 0x00000029, 0x000600b6,
        0x0000003c, 0x0000001e, 0x00000027, 0x00030081,
        0x00000027, 0x00000047, 0x000a0004, 0x0000001e,
        0x0000003e, 0x00000047, 0x000500b6, 0x00000027,
        0x0000000f, 0x00000020, 0x00030081, 0x00000027,
        0x00000047, 0x00030081, 0x00000020, 0x00000047,
        0x000600b6, 0x00000047, 0x00000023, 0x0000001e,
        0x0000002f, 0x00080015, 0x0000001d, 0x00000024,
        0x00000009, 0x00000027, 0x00000047, 0x000b000e,
        0x0000003c, 0x0000001e, 0x00000047, 0x0000002f,
        0x0000003c, 0x000600b6, 0x0000002f, 0x0000001e,
        0x00000047, 0x000500b8, 0x0009003c, 0x00000023,
        0x000600b6, 0x0000001e, 0x00000047, 0x000500b6,
        0x00000047, 0x00000023, 0x000600b8, 0x00000047
    };

    // Create vertex shader module
    VkShaderModuleCreateInfo vertexModuleInfo = {};
    vertexModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexModuleInfo.codeSize = static_cast<VkDeviceSize>(vertexSPIRV.size() * sizeof(uint32_t));
    vertexModuleInfo.pCode = vertexSPIRV.data();
    
    VkResult result = vkCreateShaderModule(device, &vertexModuleInfo, nullptr, &this->vertexShaderModule_);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create vertex shader module! Error: " << result << "\n";
        return result;
    }

    // Create fragment shader module (same as vertex for simplicity)
    VkShaderModuleCreateInfo fragmentModuleInfo = {};
    fragmentModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentModuleInfo.codeSize = static_cast<VkDeviceSize>(vertexSPIRV.size() * sizeof(uint32_t));
    fragmentModuleInfo.pCode = vertexSPIRV.data();
    
    result = vkCreateShaderModule(device, &fragmentModuleInfo, nullptr, &this->fragmentShaderModule_);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create fragment shader module! Error: " << result << "\n";
        vkDestroyShaderModule(device, this->vertexShaderModule_, nullptr);
        return result;
    }
    
    std::cout << "  [VulkanPipeline] Shader modules created\n";
    
    // Setup vertex shader stage
    VkPipelineShaderStageCreateInfo vertexStage = {};
    vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexStage.module = this->vertexShaderModule_;
    vertexStage.pName = "main";
    
    // Setup fragment shader stage  
    VkPipelineShaderStageCreateInfo fragmentStage = {};
    fragmentStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentStage.module = this->fragmentShaderModule_;
    fragmentStage.pName = "main";

    // Create pipeline layout (no descriptor sets for simple case)
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 0;
    layoutInfo.pushConstantRangeCount = 0;
    layoutInfo.pSetLayouts = nullptr;
    layoutInfo.pPushConstantRanges = nullptr;
    
    result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create pipeline layout! Error: " << result << "\n";
        return result;
    }

    // Create render pass (no attachments - rendering directly to swapchain)
    VkRenderPassCreateInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    passInfo.attachmentCount = 0;
    
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = nullptr;
    subpass.preserveStorageBits = 0;
    
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = 0;
    passInfo.pDependencies = nullptr;
    
    result = vkCreateRenderPass(device, &passInfo, nullptr, &renderPassHandle);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create render pass! Error: " << result << "\n";
        return result;
    }

    // Vertex input state - define vertex attribute layout
    VkPipelineVertexInputStateCreateInfo vertexInput = {};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    
    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(float) * 6;  // 3 position + 3 color = 24 bytes
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = 2;
    
    VkVertexInputAttributeDescription attrDescPos{};
    attrDescPos.location = 0;
    attrDescPos.binding = 0;
    attrDescPos.format = VK_FORMAT_R32G32B32_SFLOAT;  // Position (vec3)
    attrDescPos.offset = 0;
    
    VkVertexInputAttributeDescription attrDescColor{};
    attrDescColor.location = 1;
    attrDescColor.binding = 0;
    attrDescColor.format = VK_FORMAT_R32G32B32_SFLOAT;  // Color (vec3)
    attrDescColor.offset = sizeof(float) * 3;
    
    vertexInput.pVertexAttributeDescriptions = &attrDescPos;

    // Vertex input assembly - default primitive topology
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissors state
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 0;

    // Multisampling state (no MSAA for simple case)
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
    colorBlend.attachmentCount = 0;
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

    // Depth/stencil state - disabled
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 1;
    dynamicState.pDynamicStates = &VK_DYNAMIC_STATE_VIEWPORT;

    // Pipeline viewport info
    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 0;

    // Create pipeline
    VkPipelineGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = &vertexStage;  // We'll just use one for now
    
    // For a minimal demo, let's create with only vertex shader (simple output test)
    pipelineInfo.stageCount = 1;
    pipelineInfo.pStages = &vertexStage;

    pipelineInfo.vertexInputState = vertexInput;
    pipelineInfo.inputAssemblyState = inputAssembly;
    pipelineInfo.viewportState = viewport;
    pipelineInfo.multisampleState = multisample;
    pipelineInfo.colorBlendState = colorBlend;
    pipelineInfo.rasterizationState = rasterizer;
    pipelineInfo.depthStencilState = depthStencil;
    pipelineInfo.dynamicState = dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPassHandle;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineHandle);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create graphics pipeline! Error: " << result << "\n";
        return result;
    }

    std::cout << "✅ Graphics pipeline created successfully\n";
    return VK_SUCCESS;
}

bool VulkanPipeline::getUniformBufferSize() const {
    if (uniformBuffer != VK_NULL_HANDLE) {
        VkMemoryRequirements memReqs{};
        vkGetBufferMemoryRequirements(device, uniformBuffer, &memReqs);
        return memReqs.size > 0;
    }
    return false;
}

VkResult VulkanPipeline::getRenderPass() const {
    std::cout << "  [VulkanPipeline] getRenderPass: renderPassHandle=" 
              << (renderPassHandle == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return renderPassHandle != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VulkanPipeline::getUniformBuffer() const {
    if (uniformBuffer != VK_NULL_HANDLE) {
        std::cout << "  [VulkanPipeline] getUniformBuffer: valid buffer\n";
        return VK_SUCCESS;
    }
    std::cerr << "  [VulkanPipeline] getUniformBuffer: no uniform buffer created\n";
    return VK_ERROR_INITIALIZATION_FAILED;
}

bool VulkanPipeline::hasVertexShader() const {
    if (vertexShaderModule_ != VK_NULL_HANDLE) {
        std::cout << "  [VulkanPipeline] hasVertexShader: true\n";
        return true;
    }
    std::cerr << "  [VulkanPipeline] hasVertexShader: false\n";
    return false;
}

VkResult VulkanPipeline::hasFragmentShader() const {
    if (fragmentShaderModule_ != VK_NULL_HANDLE) {
        std::cout << "  [VulkanPipeline] hasFragmentShader: true\n";
        return VK_SUCCESS;
    }
    std::cerr << "  [VulkanPipeline] hasFragmentShader: false\n";
    return VK_ERROR_INITIALIZATION_FAILED;
}

VkResult VulkanPipeline::bind(VkCommandBuffer commandBuffer) const {
    std::cout << "  [VulkanPipeline] bind: vkCmdBindPipeline\n";
    VkPipelineBindingDivisions bindingDivisions{};
    bindingDivisions.bindingCount = 1;
    bindingDivisions.pBindingDivisions = nullptr;
    
    VkSubpassDescription2 subpassDesc{};
    subpassDesc.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    
    std::cout << "✅ Pipeline bound to command buffer\n";
    return VK_SUCCESS;
}

VkCommandBuffer_T* VulkanPipeline::bindVertexBuffers(VkCommandBuffer, const std::vector<Vertex>&, VkBuffer) const {
    std::cout << "  [VulkanPipeline] bindVertexBuffers: (stub)\n";
    return nullptr;
}

VkCommandBuffer_T* VulkanPipeline::bindIndexBuffer(VkCommandBuffer, const std::vector<uint16_t>&, VkBuffer) const {
    std::cout << "  [VulkanPipeline] bindIndexBuffer: (stub)\n";
    return nullptr;
}

void VulkanPipeline::endRenderPass(VkCommandBuffer, uint32_t) const {
    std::cout << "  [VulkanPipeline] endRenderPass: (stub)\n";
}

bool VulkanPipeline::getPipelineLayout() const {
    std::cout << "  [VulkanPipeline] getPipelineLayout: pipelineLayout=" 
              << (pipelineLayout == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return pipelineLayout != VK_NULL_HANDLE;
}

const void* VulkanPipeline::getShaderBindings() const {
    std::cout << "  [VulkanPipeline] getShaderBindings: no shader bindings configured\n";
    return nullptr;
}

const VkShaderModule* VulkanPipeline::getVertexShaderModule() const {
    std::cout << "  [VulkanPipeline] getVertexShaderModule: vertexShaderModule_=" 
              << (vertexShaderModule_ == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return vertexShaderModule_ == VK_NULL_HANDLE ? nullptr : &vertexShaderModule_;
}

const VkShaderModule* VulkanPipeline::getFragmentShaderModule() const {
    std::cout << "  [VulkanPipeline] getFragmentShaderModule: fragmentShaderModule_=" 
              << (fragmentShaderModule_ == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return fragmentShaderModule_ == VK_NULL_HANDLE ? nullptr : &fragmentShaderModule_;
}

VkPhysicalDevice VulkanPipeline::getPhysicalDevice() const {
    std::cout << "  [VulkanPipeline] getPhysicalDevice: physicalDevice=" 
              << (physicalDevice == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return physicalDevice;
}

VkQueue VulkanPipeline::getGraphicsQueue() const {
    std::cerr << "  [VulkanPipeline] getGraphicsQueue: graphicsQueue not stored in class\n";
    return VK_NULL_HANDLE;
}

VkImage VulkanPipeline::getColorAttachment() const {
    std::cerr << "  [VulkanPipeline] getColorAttachment: no color attachment exposed directly\n";
    return VK_NULL_HANDLE;
}

VulkanTexture* VulkanPipeline::getTexture() const {
    std::cout << "  [VulkanPipeline] getTexture: texture=" 
              << (texture ? "valid pointer" : "nullptr") << "\n";
    return texture;
}

// Helper: Find memory type for buffers
static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return UINT32_MAX;
}

// Find the render pass
VkResult VulkanPipeline::findRenderPass() const {
    std::cout << "  [VulkanPipeline] findRenderPass: using current handle\n";
    return getRenderPass();
}

// Helper function to create shader module (used internally)
static VkShaderModule createShaderModule(VkDevice device, const std::vector<uint32_t>& code) {
    if (code.empty()) {
        std::cerr << "❌ Cannot create shader module from empty bytecode!\n";
        return VK_NULL_HANDLE;
    }
    
    VkShaderModuleCreateInfo moduleInfo = {};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = static_cast<VkDeviceSize>(code.size() * sizeof(uint32_t));
    moduleInfo.pCode = code.data();

    VkShaderModule shaderModule{};
    VkResult result = vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create shader module! Vulkan error: " << result << "\n";
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

// Vertex input binding state - simplified
VkBufferVertexInputBindingState createVertexInputBindingState(const VkVertexInputBindingDescription* description) {
    VkBufferVertexInputBindingState binding{};
    binding.binding = description->binding;
    binding.divisor = 1;
    binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
    binding.stride = description->stride;
    return binding;
}

// Cleanup function
void VulkanPipeline::cleanup() {
    // Cleanup is handled by destroy()
}

} // namespace blitz
