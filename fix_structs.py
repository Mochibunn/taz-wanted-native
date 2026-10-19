#!/usr/bin/env python3
import re
import sys

# Read the file
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    content = f.read()

# Fix VkBufferMemoryRequirements -> VkMemoryRequirements
content = content.replace('VkBufferMemoryRequirements buffer_mem_reqs{};', 
                          'VkMemoryRequirements mem_reqs{};')
content = content.replace('&buffer_mem_reqs;', '&mem_reqs;')

# Add struct definitions before main function - we need to add these static objects
# Find the location right after g_swapchain declaration (~line 60)
struct_def = '''
// Static state objects for pipeline creation (required for address-of operator)
VkVertexInputStateCreateInfo vertex_input_state{};
vertex_input_state.sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_STATE_CREATE_INFO;
vertex_input_state.vertexBindingDescriptionCount = 0;

VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state{};
pipeline_vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
pipeline_vertex_input_state.pVertexBindingDescriptions = nullptr;
pipeline_vertex_input_state.vertexBindingDescriptionCount = 0;

VkPipelineInputAssemblyStateCreateInfo input_assembly_state{};
input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

VkViewport viewport{};
viewport.width = WINDOW_WIDTH;
viewport.height = WINDOW_HEIGHT;
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;
VkPipelineViewportStateCreateInfo viewport_state{};
viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewport_state.viewportCount = 1;
viewport_state.pViewports = &viewport;

VkPipelineRasterizationStateCreateInfo rasterization_state{};
rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterization_state.rasterizerDiscardEnable = VK_FALSE;
rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
rasterization_state.lineWidth = 1.0f;

VkPipelineMultisampleStateCreateInfo multisample_state{};
multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// Color blend state
uint32_t color_blend_attachment_count = 1;
VkPipelineColorBlendAttachmentState color_blend_attachment{};
color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
color_blend_attachment.blendEnable = VK_FALSE;

std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments(color_blend_attachment_count);
color_blend_attachments[0] = color_blend_attachment;

VkPipelineColorBlendStateCreateInfo color_blend_state{};
color_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
color_blend_state.logicOpEnable = VK_FALSE;
color_blend_state.attachmentCount = static_cast<uint32_t>(color_blend_attachments.size());
color_blend_state.pAttachments = color_blend_attachments.data();

VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};
depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
depth_stencil_state.depthTestEnable = VK_FALSE;
depth_stencil_state.depthWriteEnable = VK_FALSE;
depth_stencil_state.depthCompareOp = VK_COMPARE_OP_ALWAYS;

'''

# Insert the struct definitions after line 65 (after g_swapchain declaration)
lines = content.split('\n')
insert_pos = None
for i, line in enumerate(lines):
    if 'VkSwapchainKHR g_swapchain = VK_NULL_HANDLE;' in line:
        insert_pos = i + 1
        break

if insert_pos:
    lines.insert(insert_pos, struct_def)
    content = '\n'.join(lines)
else:
    print("Could not find insertion point", file=sys.stderr)
    sys.exit(1)

# Fix shader module creation to use static object or return value properly
content = content.replace('shader_module_create_info(', 'VkShaderModuleCreateInfo{};')

# Fix vertex shader creation
content = content.replace('vertex_shader_create', 'vertex_shader_ci')

# Fix fragment shader creation  
content = content.replace('fragment_shader_create', 'fragment_shader_ci')

# Write the fixed file
with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.write(content)

print("Fixed struct definitions")
