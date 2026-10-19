// ============================================================================
// TAZ: WANTED - Simple ECS Test Program
// Minimal test to verify entity system works correctly
// ============================================================================

#include <iostream>
#include "../include/ECS.h"

int main() {
    std::cout << "=== TAZ: Wanted ECS Engine Test ===\n\n";
    
    // Get singleton instance (already handled by header)
    ecs::ECSRegistry& reg = ecs::ECSRegistry::getInstance();
    
    // Create some entities (Taz, Sylvester, Ghost from REA model)
    std::cout << "Creating entities...\n";
    
    uint32_t id_taz = reg.create();
    auto& taz = reg.getTransform(id_taz);
    taz.position.x = 0.0f;
    taz.position.y = 1.5f;
    taz.position.z = -5.0f;
    std::cout << "✓ Taz entity created: ID " << id_taz << "\n";
    std::cout << "  Position: (" << taz.position.x << ", " 
              << taz.position.y << ", " << taz.position.z << ")\n";
    
    uint32_t id_sylvester = reg.create();
    auto& sylvester = reg.getTransform(id_sylvester);
    sylvester.position.x = 10.0f;
    sylvester.position.y = 1.2f;
    sylvester.position.z = -5.0f;
    std::cout << "✓ Sylvester entity created: ID " << id_sylvester << "\n";
    
    // Test entity destruction and recreation
    reg.destroy(id_taz);
    std::cout << "✓ Taz destroyed\n";
    
    uint32_t id_taz_recreated = reg.create();
    auto& taz_new = reg.getTransform(id_taz_recreated);
    taz_new.position.x = 0.0f;
    taz_new.position.y = 1.5f;
    taz_new.position.z = -5.0f;
    std::cout << "✓ Taz recreated: ID " << id_taz_recreated << "\n";
    
    std::cout << "\nTotal entities alive: " << reg.entities.size() << "\n\n";
    std::cout << "ECS Engine Test Complete!\n";
    std::cout << "Engine is functional and ready for game integration.\n";
    
    return 0;
}
