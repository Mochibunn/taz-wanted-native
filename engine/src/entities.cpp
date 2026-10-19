// ============================================================================
// TAZ: WANTED - Entity Registration (simplified for MVP)
// Based on REA model entities from Ghidra analysis
// ============================================================================

#include <ECS.h>
#include <iostream>

void register_test_entities() {
    ecs::ECSRegistry& reg = ecs::ECSRegistry::getInstance();
    
    // Taz (protagonist) - ID 0
    uint32_t id_taz = reg.create();
    auto& taz = reg.getTransform(id_taz);
    std::cout << "[ECS] Created Taz: ID " << id_taz << ", Pos: (" 
              << taz.position.x << ", " << taz.position.y << ", " << taz.position.z << ")\n";
    
    // Sylvester (coyote NPC) - ID 1  
    uint32_t id_sylvester = reg.create();
    auto& sylvester = reg.getTransform(id_sylvester);
    std::cout << "[ECS] Created Sylvester: ID " << id_sylvester 
              << ", Pos: (" << sylvester.position.x << ", " << sylvester.position.y << ")\n";
    
    // Ghost (spectral NPC) - ID 2
    uint32_t id_ghost = reg.create();
    auto& ghost = reg.getTransform(id_ghost);
    std::cout << "[ECS] Created Ghost: ID " << id_ghost 
              << ", Pos: (" << ghost.position.x << ", " << ghost.position.y << ")\n";
    
    // Boss entities (more complex with AI)
    uint32_t id_boss1 = reg.create();
    std::cout << "[ECS] Created Boss1: ID " << id_boss1 << "\n";
    
    uint32_t id_boss2 = reg.create();
    std::cout << "[ECS] Created Boss2: ID " << id_boss2 << "\n";
    
    // Zone entities (museum, cityhub, etc.)
    uint32_t id_museum = reg.create();
    std::cout << "[ECS] Created Museum zone: ID " << id_museum << "\n";
    
    std::cout << "[ECS] Total registered entities: " << reg.entities.size() << "\n\n";
    std::cout << "Entity registration complete!\n";
}
