// ============================================================================
// TAZ: WANTED - Entity Component System Header (ECS.h)
// Simplified working version for Linux port
// ============================================================================

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <stdexcept>

// Simple math types (no GLM dependency for simple ECS demo)
namespace ecs {

using vec3 = struct { float x, y, z; };

struct Transform {
    vec3 position{};
    vec3 rotation{};
    vec3 scale{};
};

struct Mesh {
    uint32_t vertex_buffer{};
    uint32_t index_count{};
};

struct Audio {
    float volume{};
    bool looping = false;
};

struct Health {
    float current_health{};
    float max_health{};
};

struct AIState {
    enum class State { Idle, Patrol, Chase, Attack } state{State::Idle};
};

// ============================================================================
// ENTITY STORAGE - Singleton Registry Pattern
// ============================================================================

class ECSRegistry {
public:
    // Singleton pattern - returns same instance
    static ECSRegistry& getInstance() {
        static ECSRegistry instance;
        return instance;
    }
    
    // Create new entity with sequential ID
    uint32_t create() {
        uint32_t id = this->entity_count++;
        Transform default_transform{};
        default_transform.position.x = 0.0f;
        default_transform.position.y = 1.5f;  // Player height
        default_transform.position.z = -5.0f;
        this->entities[id] = default_transform;
        return id;
    }
    
    // Destroy entity and cleanup
    void destroy(uint32_t id) {
        if (this->entities.find(id) != this->entities.end()) {
            this->entities.erase(id);
        }
    }
    
    // Check if entity exists
    bool has(uint32_t id) const {
        return this->entities.find(id) != this->entities.end();
    }
    
    // Component accessors - returns reference or throws on not found
    Transform& getTransform(uint32_t id) {
        auto it = this->entities.find(id);
        if (it == this->entities.end()) throw std::runtime_error("Entity not found");
        return it->second;
    }
    
    const Transform& getTransform(uint32_t id) const {
        auto it = this->entities.find(id);
        if (it == this->entities.end()) throw std::runtime_error("Entity not found");
        return it->second;
    }

private:
    uint32_t entity_count = 0;
    
public: // Make visible for entities.cpp
    std::unordered_map<uint32_t, Transform> entities;
};

} // namespace ecs
