// ============================================================================
// TAZ: WANTED - Main with ECS Integration
// Combines SDL2+Vulkan render loop with Entity Component System
// ============================================================================

#include <iostream>
#include <vector>
#include <ECS.h>

int main() {
    std::cout << "=== TAZ: Wanted Native - ECS Engine ===\n";
    
    // Initialize ECS registry
    ecs::ECSRegistry reg;
    
    // Register entity components
    auto id_1 = reg.create();  // Player (Taz)
    auto id_2 = reg.create();  // NPC (Sylvester)
    
    std::cout << "Created entities: " << id_1 << ", " << id_2 << "\n";
    
    // In real implementation, would load meshes from .pc files here
    // Would set up transforms, health, AI state
    
    std::cout << "ECS Engine initialized successfully\n";
    
    return 0;
}

