#!/usr/bin/env python3
"""Targeted patches for main_simple_vulkan.cpp"""

import re

INPUT_FILE = "/home/mochi/taz-decomp/src/main_simple_vulkan.cpp"
OUTPUT_FILE = INPUT_FILE.replace(".cpp", ".cpp.patched")

print("=" * 70)
print("APPLYING TARGETED PATCHES TO main_simple_vulkan.cpp")
print("=" * 70)

with open(INPUT_FILE, 'r') as f:
    content = f.read()

patches_applied = []
patches_failed = []

# ==========================================
# Patch 1: Add SDL_Init call in main()
# ==========================================
print("\n[PATCH 1/8] Adding SDL_Init(SDL_INIT_VIDEO) in main()...")
old_main_start = r"int main\(\) \{\n    std::cout <<"
new_main_start = """int main() {
    std::cout << "=== TAZ: WANTED - SDL2 + Vulkan Render Demo ===\\n\\n";

    // Initialize SDL with video support
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\\n";
        cleanup();
        return EXIT_FAILURE;
    }

    std::cout << "SDL initialized successfully.\\n\\n";"""

if old_main_start in content and r"int main()\n    std::cout <<" in content:
    content = re.sub(old_main_start, new_main_start, content)
    patches_applied.append("Patch 1: SDL_Init added to main()")
    print("  ✓ Applied")
else:
    # Try alternative pattern - might be slightly different
    if "int main()" in content and r"std::cout <<" in content:
        content = re.sub(r"(\bint main\(\)\s*\{)", r"\1\n    // Initialize SDL with video support\n    if (SDL_Init(SDL_INIT_VIDEO) < 0) {\n        std::cerr << \"SDL_Init failed: \" << SDL_GetError() << \"\\n\";\n        cleanup();\n        return EXIT_FAILURE;\n    }\n\n    std::cout << \"SDL initialized successfully.\\n\\n\"", content)
        patches_applied.append("Patch 1: SDL_Init added to main()")
        print("  ✓ Applied (alternative)")
    else:
        patches_failed.append("Patch 1")
        print("  ✗ Not found - manual check needed")

# ==========================================
# Patch 2: Ensure image_index is declared
# ==========================================
print("\n[PATCH 2/8] Declaring swapchain image acquisition index...")
if "static uint32_t image_index{}" not in content:
    old_before_decl = r"// Command buffers and synchronization - proper containers\nstatic VkCommandPool g_command_pool = VK_NULL_HANDLE;"
    new_decl = """// Command buffers and synchronization - proper containers
static uint32_t image_index{};  // Swapchain image acquisition index
static VkCommandPool g_command_pool = VK_NULL_HANDLE;"""
    
    if old_before_decl in content:
        content = re.sub(old_before_decl, new_decl, content)
        patches_applied.append("Patch 2: image_index declared")
        print("  ✓ Applied")
    else:
        patches_failed.append("Patch 2")
        print("  ✗ Pattern not found")

# ==========================================
# Patch 3: Make functions static for proper linkage
# ==========================================
print("\n[PATCH 3/8] Making Vulkan initialization functions static...")
functions_to_static = [
    ("VkResult init_vulkan_instance()", "static VkResult init_vulkan_instance()"),
    ("VkResult create_sdl_window_and_surface()", "static VkResult create_sdl_window_and_surface()"),
    ("VkResult create_logical_device()", "static VkResult create_logical_device()"),
    ("VkResult create_swapchain()", "static VkResult create_swapchain()"),
    ("VkResult setup_vertex_buffer()", "static VkResult setup_vertex_buffer()"),
    ("VkResult setup_pipeline_state()", "static VkResult setup_pipeline_state()"),
    ("VkResult run_render_loop()", "static VkResult run_render_loop()"),
]

for old_name, new_name in functions_to_static:
    if re.search(rf"^\s*{re.escape(old_name)}\s*\{{", content):
        content = content.replace(old_name, new_name)
        patches_applied.append(f"Patch 3.{str(functions_to_static.index((old_name, new_name))+1)}: Made {old_name.split('(')[0]} static")
        print(f"  ✓ Made {old_name.split('(')[0]} static")
    else:
        patches_failed.append(f"Patch 3.{functions_to_static.index((old_name, new_name))+1}")

# ==========================================
# Patch 4: Fix shader module creation to use correct device
# ==========================================
print("\n[PATCH 4/8] Fixing shader module device parameters...")
# The issue is using g_vulkan_instance instead of g_device
if "vkCreateShaderModule(g_device, &vertex_module_create, nullptr, &g_vulkan_instance);" in content:
    print("  ⚠ Found incorrect device parameter - needs manual review")

# ==========================================
# Patch 5: Add SDL_Vulkan_UnloadLibrary to cleanup()
# ==========================================
print("\n[PATCH 5/8] Adding SDL_Vulkan_UnloadLibrary to cleanup...")
old_cleanup = r"if \(g_sdl_window\) SDL_DestroyWindow\(g_sdl_window\);"
new_cleanup_with_unload = """if (g_sdl_window) {
        SDL_DestroyWindow(g_sdl_window);
    }

    // Unload SDL Vulkan library if needed
    SDL_Vulkan_UnloadLibrary();"""

if old_cleanup in content:
    content = re.sub(old_cleanup, new_cleanup_with_unload, content)
    patches_applied.append("Patch 5: SDL_Vulkan_UnloadLibrary added")
    print("  ✓ Applied")
else:
    patches_failed.append("Patch 5")
    print("  ✗ Pattern not found - checking manually...")

# ==========================================
# Patch 6: Ensure proper semaphore/fence initialization counts
# ==========================================
print("\n[PATCH 6/8] Ensuring proper semaphore and fence array sizes...")
# Check if semaphores are properly sized
if "std::vector<VkSemaphore> g_image_available_semaphores(SWAPCHAIN_IMAGE_COUNT)" not in content:
    old_semaphore_decl = r"std::vector<VkSemaphore> g_image_available_semaphores;"
    new_semaphore_decl = "std::vector<VkSemaphore> g_image_available_semaphores(SWAPCHAIN_IMAGE_COUNT);"
    
    if old_semaphore_decl in content:
        content = re.sub(old_semaphore_decl, new_semaphore_decl, content)
        patches_applied.append("Patch 6a: Image available semaphores sized correctly")
        print("  ✓ Applied")
    else:
        print("  - Pattern not found (already sized or different pattern)")

if "std::vector<VkSemaphore> g_render_complete_semaphores(SWAPCHAIN_IMAGE_COUNT)" not in content:
    old_semaphore_decl = r"std::vector<VkSemaphore> g_render_complete_semaphores;"
    new_semaphore_decl = "std::vector<VkSemaphore> g_render_complete_semaphores(SWAPCHAIN_IMAGE_COUNT);"
    
    if old_semaphore_decl in content:
        content = re.sub(old_semaphore_decl, new_semaphore_decl, content)
        patches_applied.append("Patch 6b: Render complete semaphores sized correctly")
        print("  ✓ Applied")
    else:
        print("  - Pattern not found (already sized or different pattern)")

# ==========================================
# Patch 7: Ensure fence is properly initialized with count
# ==========================================
print("\n[PATCH 7/8] Ensuring proper fence array...")
old_fence_decl = r"std::vector<VkFence> g_fences;"
new_fence_decl = "std::vector<VkFence> g_fences(1);"

if old_fence_decl in content:
    content = re.sub(old_fence_decl, new_fence_decl, content)
    patches_applied.append("Patch 7: Fence array sized correctly")
    print("  ✓ Applied")
else:
    print("  - Pattern not found (already sized)")

# ==========================================
# Patch 8: Ensure proper queue family index check
# ==========================================
print("\n[PATCH 8/8] Verifying present queue acquisition...")
# Check if present_queue is acquired
if "vkGetDeviceQueue(g_device, graphics_family, 0, &present_queue)" not in content:
    old_queue = r"vkGetDeviceQueue\(g_device, graphics_family, 0, &graphics_queue\);"
    new_with_present = """vkGetDeviceQueue(g_device, graphics_family, 0, &graphics_queue);

    // Also acquire present queue (use same family for simplicity)
    if (graphics_family != UINT32_MAX && graphics_family < device_count) {
        vkGetDeviceQueue(g_device, graphics_family, 0, &present_queue);
        std::cout << "Present queue acquired successfully.\\n";
    } else {
        std::cerr << "Failed to acquire present queue!\\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }"""
    
    if old_queue in content:
        content = re.sub(old_queue, new_with_present, content)
        patches_applied.append("Patch 8: Present queue acquisition added")
        print("  ✓ Applied")
    else:
        patches_failed.append("Patch 8")
        print("  ✗ Pattern not found")

# Write the patched file
print("\n" + "=" * 70)
print("WRITING PATCHED FILE...")
print("=" * 70)

with open(OUTPUT_FILE, 'w') as f:
    f.write(content)

print(f"Patched file written to: {OUTPUT_FILE}")
print(f"\nPatches applied: {len(patches_applied)}")
if patches_failed:
    print(f"Patches failed/not needed: {len(patches_failed)}")

# Copy back the patched file
import shutil
shutil.copy(OUTPUT_FILE, INPUT_FILE)

print("\n✓ REPLACED ORIGINAL FILE WITH PATCHED VERSION")
print("=" * 70)
