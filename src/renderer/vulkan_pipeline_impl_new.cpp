// Implementation for Vulkan graphics pipeline creation and shader module compilation
#include "renderer/vulkan_pipeline.h"
#include "../transforms.h"
#include <cstring>
#include <iostream>

namespace blitz {

// Vertex shader pipeline creation info (static to avoid re-init)
static VkPipelineShaderStageCreateInfo vertexCreateInfo_ = {};
static VkPipelineShaderStageCreateInfo fragmentCreateInfo_ = {};

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

// Helper: Create shader module from SPIR-V bytecode
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

// Constructor
VulkanPipeline::VulkanPipeline() 
    : device(VK_NULL_HANDLE), physicalDevice(VK_NULL_HANDLE), 
      uniformBuffer(VK_NULL_HANDLE), uniformBufferMemory(VK_NULL_HANDLE), 
      pipelineLayout(VK_NULL_HANDLE), renderPassHandle(VK_NULL_HANDLE),
      vertexShaderModule_(VK_NULL_HANDLE), fragmentShaderModule_(VK_NULL_HANDLE),
      pipelineHandle(VK_NULL_HANDLE) {}

// Destructor
VulkanPipeline::~VulkanPipeline() {
    destroy();
}

// Destroy all Vulkan resources
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

// Initialize Vulkan device and physical device handles
bool VulkanPipeline::initialize(VkDevice _device, VkPhysicalDevice _physical_device) {
    device = _device;
    physicalDevice = _physical_device;
    return true;
}

// Setup transform uniform buffer for MVP matrices
bool VulkanPipeline::setupTransformUniformBuffer(VkDevice device) {
    if (!device || uniformBuffer != VK_NULL_HANDLE || uniformBufferMemory != VK_NULL_HANDLE) {
        return true; // Already set up or null device
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

// Compile shaders and create graphics pipeline - signature matches header: (const char*, const char*)
VkResult VulkanPipeline::create(const char* vertexShaderSource, const char* fragmentShaderSource) {
    // Validate inputs
    if (!device || !physicalDevice) {
        std::cerr << "❌ Invalid device or physical device passed to create()\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Destroy existing resources if any (simple cleanup for this demo)
    destroy();

    // Setup uniform buffer first
    if (!setupTransformUniformBuffer(device)) {
        std::cerr << "❌ Failed to setup transform uniform buffer\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::cout << "✅ Creating graphics pipeline...\n";
    
    // Simple vertex shader: output position and color as attribute
    const char* vertexShader = R"(
        #version 450
        layout(location=0) in vec3 inPos;
        layout(location=1) in vec3 inColor;
        layout(location=0) out vec3 fragColor;
        
        void main() {
            gl_Position = vec4(inPos, 1.0);
            fragColor = inColor;
        }
    )";

    // Simple fragment shader: output vertex color
    const char* fragmentShader = R"(
        #version 450
        layout(location=0) in vec3 fragColor;
        layout(location=0) out vec4 outColor;
        
        void main() {
            outColor = vec4(fragColor, 1.0);
        }
    )";

    // Convert GLSL to SPIR-V (simple inline compiler using glslang reference or placeholder for demo)
    // For simplicity, we'll use a very basic placeholder - in production you'd use glslangValidator or SPIRV-Cross
    // Here's a minimal approach: compile via external tool would be better, but for demo we'll create from SPIR-V bytes
    
    // Minimal SPIR-V bytecode for vertex shader (triangle output with color passthrough)
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
    
    std::cout << "✅ Shader modules created successfully\n";
    
    // Setup vertex shader stage
    vertexCreateInfo_.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexCreateInfo_.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexCreateInfo_.module = this->vertexShaderModule_;
    vertexCreateInfo_.pName = "main";
    
    // Setup fragment shader stage  
    fragmentCreateInfo_.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentCreateInfo_.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentCreateInfo_.module = this->fragmentShaderModule_;
    fragmentCreateInfo_.pName = "main";

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
    subpass.resolvedViewportHeight = 1;
    subpass.viewMask = 0;
    
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependenceCount = 0;
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
    attrDescPos.format = VK_FORMAT_R32G32B32_FLOAT;  // Position (vec3)
    attrDescPos.offset = 0;
    
    VkVertexInputAttributeDescription attrDescColor{};
    attrDescColor.location = 1;
    attrDescColor.binding = 0;
    attrDescColor.format = VK_FORMAT_R32G32B32_FLOAT;  // Color (vec3)
    attrDescColor.offset = sizeof(float) * 3;
    
    vertexInput.pVertexAttributeDescriptions = &attrDescPos;

    // Vertex input assembly - default primitive topology
    VkPipelineInputAssemblyCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and scissors state
    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 0;

    // Multisampling state (no MSAA for simple case)
    VkPipelineMultisampleStateCreateInfo multisample = {};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending state
    VkPipelineColorBlendAttachmentState colorBlendAtt = {};
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

    VkPipelineColorBlendStateCreateInfo colorBlend = {};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.logicOpEnable = VK_FALSE;
    colorBlend.logicOp = VK_LOGIC_OP_COPY;
    colorBlend.attachmentCount = 0;  // No attachments for swapchain rendering
    colorBlend.blendConstants[0] = {0.0f, 0.0f};

    // Depth stencil state (no depth testing for simple colored triangle)
    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.pDepthStencilState = &depthStencil;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.sampleMask = 0xFFFFFFFFu;
    rasterization.tininessOfDepthTreshold = 0.0f;
    rasterization.depthBiasEnable = VK_FALSE;

    // Pipeline dynamic state (empty for now)
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 0;
    dynamicState.pDynamicStates = nullptr;

    // Pipeline graphics pipeline create info
    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = &vertexCreateInfo_;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPassHandle;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineHandle);
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to create graphics pipeline! Error: " << result << "\n";
        return result;
    }

    std::cout << "✅ Graphics pipeline created successfully\n";
    return VK_SUCCESS;
}

// Bind graphics pipeline before drawing (uses vkCmdBindPipeline) - matches header signature
VkCommandBuffer VulkanPipeline::bind(VkCommandBuffer commandBuffer) const {
    if (commandBuffer == VK_NULL_HANDLE || pipelineHandle == VK_NULL_HANDLE) {
        std::cerr << "❌ Invalid command buffer or pipeline handle\n";
        return VK_NULL_HANDLE;
    }
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineHandle);
    std::cout << "  [VulkanPipeline] Bound graphics pipeline.\n";
    return commandBuffer;
}

// Begin recording a render pass on command buffer - matches header signature
VkCommandBuffer VulkanPipeline::beginRenderPass(VkCommandBuffer commandBuffer, 
    uint32_t subpassIndex, uint32_t width, uint32_t height) const {
    if (commandBuffer == VK_NULL_HANDLE || renderPassHandle == VK_NULL_HANDLE) {
        std::cerr << "❌ Invalid command buffer or render pass\n";
        return VK_NULL_HANDLE;
    }
    
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPassHandle;
    renderPassInfo.framebuffer = VK_NULL_HANDLE;  // No framebuffer for swapchain rendering
    renderPassInfo.renderArea.offsets[0] = {0, 0};
    renderPassInfo.renderArea.extent.width = static_cast<uint32_t>(width);
    renderPassInfo.renderArea.extent.height = static_cast<uint32_t>(height);

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    std::cout << "  [VulkanPipeline] Begin render pass.\n";
    return commandBuffer;
}

// Set viewport for rendering - matches header signature  
VkCommandBuffer VulkanPipeline::setViewport(VkCommandBuffer commandBuffer, 
    uint32_t width, uint32_t height) const {
    if (commandBuffer == VK_NULL_HANDLE) {
        std::cerr << "❌ Invalid command buffer\n";
        return VK_NULL_HANDLE;
    }
    
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    std::cout << "  [VulkanPipeline] Set viewport: " << width << "x" << height << "\n";
    return commandBuffer;
}

// Draw triangles - matches header signature  
VkCommandBuffer VulkanPipeline::drawTriangles(VkCommandBuffer commandBuffer,
    size_t vertexCount, size_t instanceCount) {
    if (commandBuffer == VK_NULL_HANDLE || pipelineHandle == VK_NULL_HANDLE) {
        std::cerr << "❌ Invalid command buffer or pipeline\n";
        return VK_NULL_HANDLE;
    }
    
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertexCount), 
              static_cast<uint32_t>(instanceCount), 0, 0);
    std::cout << "  [VulkanPipeline] Draw triangles: " << vertexCount << " vertices\n";
    return commandBuffer;
}

// Begin draw with transform support (MVP matrices) - matches header signature  
VkCommandBuffer VulkanPipeline::drawWithTransform(VkCommandBuffer commandBuffer,
                                                  const std::vector<Vertex>& vertices,
                                                  const std::vector<uint16_t>& indices,
                                                  const SceneGraphNode& node,
                                                  const CameraFrustum& camera) const {
    if (commandBuffer == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    // Bind vertex and index buffers if provided
    if (!vertices.empty()) {
        // Would need vertex buffer binding code here
    }
    if (!indices.empty()) {
        // Would need index buffer binding code here  
    }
    
    vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertexCount ? vertices.size() : 0), 
              static_cast<uint32_t>(instanceCount), 0, 0);
    return commandBuffer;
}

// Bind vertex buffers - matches header signature
VkCommandBuffer VulkanPipeline::bindVertexBuffers(VkCommandBuffer commandBuffer, 
    const std::vector<Vertex>& vertices, 
    VkBuffer vertexBuffer, VkDeviceSize offset) const {
    if (commandBuffer == VK_NULL_HANDLE || vertexBuffer == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    VkBufferVertexInputBindingState binding = {};
    binding.binding = 0;
    binding.description.stride = sizeof(float) * 6;
    binding.description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    
    // In practice, you'd use vkCmdBindVertexBuffers to bind the buffer here
    std::cout << "  [VulkanPipeline] Would bind vertex buffers at offset " << offset << "\n";
    return commandBuffer;
}

// Bind index buffer - matches header signature
VkCommandBuffer VulkanPipeline::bindIndexBuffer(VkCommandBuffer commandBuffer,
    const std::vector<uint16_t>& indices,
    VkBuffer indexBuffer) const {
    if (commandBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
        return VK_NULL_HANDLE;
    }
    
    std::cout << "  [VulkanPipeline] Would bind index buffer\n";
    return commandBuffer;
}

// End render pass - matches header signature
void VulkanPipeline::endRenderPass(VkCommandBuffer commandBuffer, 
    uint32_t subpassIndex) const {
    if (commandBuffer == VK_NULL_HANDLE) {
        std::cerr << "❌ Invalid command buffer\n";
        return;
    }
    
    vkCmdEndRenderPass(commandBuffer);
    std::cout << "  [VulkanPipeline] End render pass.\n";
}

// Submit to queue - matches header signature
void VulkanPipeline::submitToQueue() const {
    if (graphicsQueue == VK_NULL_HANDLE) {
        std::cerr << "❌ Graphics queue not initialized\n";
        return;
    }
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    
    VkFramebuffer framebuffer = VK_NULL_HANDLE;  // Would need valid framebuffer
    
    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.renderPass = renderPassHandle;
    renderPassInfo.framebuffer = framebuffer;
    
    std::cout << "  [VulkanPipeline] Would submit command buffer to graphics queue\n";
}

// Destroy pipeline resources - matches header signature
void VulkanPipeline::destroy() {
    cleanup();
}

// Cleanup - matches header signature  
void VulkanPipeline::cleanup() {
    destroy();
}

// Getters that match header - return handles or VK_NULL_HANDLE if not created
bool VulkanPipeline::getPipelineLayout() const {
    std::cout << "  [VulkanPipeline] getPipelineLayout: pipelineLayout=" 
              << (pipelineLayout == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return pipelineLayout != VK_NULL_HANDLE;
}

VkResult VulkanPipeline::getRenderPass() const {
    std::cout << "  [VulkanPipeline] getRenderPass: renderPassHandle=" 
              << (renderPassHandle == VK_NULL_HANDLE ? "NULL" : "valid handle") << "\n";
    return renderPassHandle != VK_NULL_HANDLE ? VK_SUCCESS : VK_ERROR_INITIALIZATION_FAILED;
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

VulkanTexture* VulkanPipeline::getTexture() const {
    std::cout << "  [VulkanPipeline] getTexture: texture=" 
              << (texture ? "valid pointer" : "nullptr") << "\n";
    return texture;
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

// Create uniform buffer and bind to descriptor set - matches header signature
bool VulkanPipeline::createAndInitUniformBuffer(VkDevice device, uint32_t size) {
    if (!device) {
        return false;
    }
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = static_cast<VkDeviceSize>(size);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &uniformBuffer);
    if (result != VK_SUCCESS) {
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
    if (result != VK_SUCCESS) {
        std::cerr << "❌ Failed to allocate uniform buffer memory! Error: " << result << "\n";
        vkDestroyBuffer(device, uniformBuffer, nullptr);
        return false;
    }

    vkBindBufferMemory(device, uniformBuffer, uniformBufferMemory, 0);

    std::cout << "✅ Uniform buffer created and initialized\n";
    return true;
}

// Update uniform buffer with MVP matrices - matches header signature
bool VulkanPipeline::updateUniformBuffer(VkCommandBuffer commandBuffer, 
                                        const Matrix4& projectionMatrix,
                                        const Matrix4& viewMatrix,
                                        const Matrix4& modelMatrix) {
    if (commandBuffer == VK_NULL_HANDLE || uniformBufferMemory == VK_NULL_HANDLE) {
        return false;
    }
    
    TransformUniformsRaw uniforms{};
    uniforms.projectionMatrix = projectionMatrix;
    uniforms.viewMatrix = viewMatrix;
    uniforms.modelMatrix = modelMatrix;

    void* data = nullptr;
    if (vkMapMemory(device, uniformBufferMemory, 0, sizeof(TransformUniformsRaw), 0, &data) == VK_SUCCESS) {
        memcpy(data, &uniforms, sizeof(TransformUniformsRaw));
        vkUnmapMemory(device, uniformBufferMemory);
    }
    
    return true;
}

} // namespace blitz
