#!/usr/bin/env python3

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    content = f.read()

# Fix render pass dependencies for Vulkan 1.x
content = content.replace(
    '.resolveAttachmentCount = 0;',
    ''
)

content = content.replace(
    'rpci.pDependenciesCount = 1;',
    'rpci.dependencyCount = 1;'
)

content = content.replace(
    '.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY,',
    '.sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_KHR,'
)

# Fix access masks for Vulkan 1.x
content = content.replace(
    'VK_ACCESS_COLOR_WRITE_BIT',
    'VK_ACCESS_MEMORY_WRITE_BIT'
)

# Also fix the buffer memory requirements call
content = content.replace(
    'vkGetBufferMemoryRequirements(g_device, vertex_buffer, &buffer_mem_reqs);',
    'vkGetBufferMemoryRequirements(g_device, vertex_buffer, &mem_reqs);'
)

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.write(content)

print("Fixed Vulkan 1.x compatibility issues")
