// ============================================================================
// VULKAN HELPER HEADER
// ============================================================================
#ifndef VULKAN_HEADER_HPP
#define VULKAN_HEADER_HPP

#include <vulkan/vulkan.h>

// Helper macro for version numbers
#define MK_VERSION(major, minor, patch) ((major << 16) | (minor << 8) | patch)
#define VK_API_VERSION_1_0 0x1000000
#define VK_API_VERSION_1_1 0x1010000
#define VK_API_VERSION_1_2 0x1020000

// Helper struct for shader modules (alternative to raw SPIR-V loading)
struct VkShaderModule {
    VkShaderModule module = VK_NULL_HANDLE;
};

// ============================================================================
// UTILITY FUNCTIONS (simplified - actual implementations in source files)
// ============================================================================

#endif // VULKAN_HEADER_HPP
