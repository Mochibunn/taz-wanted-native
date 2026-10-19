#!/usr/bin/env python3

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'r') as f:
    lines = f.readlines()

# Find the line to start replacement at (after pSubpasses vector closes)
start_idx = None
for i, line in enumerate(lines):
    if 'rpci.pSubpasses[0]' in line and ';' not in line:
        start_idx = i
        break

if start_idx is None:
    print("Could not find target line")
    exit(1)

print(f"Found at line {start_idx}: {repr(lines[start_idx])}")

# Now I'll replace this broken section with a complete, correct one
replacement = [
    '    };\n',  # Close the pSubpasses vector properly\n
    '\n',\n
    'VkSemaphoreCreateInfo semaphore_create{};\n',
    'semaphore_create.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;\n',
    'VkFenceCreateInfo fence_create{};\n',
    'fence_create.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;\n',
    '\n',
    'rpci.dependencyCount = 1;\n',
    'rpci.pDependencies = &VkSubpassDependency{\n',
    '    .sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY,\n',
    '    .srcSubpass = VK_SUBPASS_EXTERNAL,\n',
    '    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,\n',
    '    .dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT,\n',
    '    .srcAccessMask = VK_ACCESS_HOST_READ_BIT\n',
    '};\n'
]

new_lines = lines[:start_idx] + replacement + lines[start_idx+1:]

with open('/home/mochi/taz-decomp/src/main_simple_vulkan.cpp', 'w') as f:
    f.writelines(new_lines)

print("Fixed render pass section")
