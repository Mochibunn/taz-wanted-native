#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include <string>
#include <vector>
#include <cstdint>

namespace blitz {

struct Vertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float u = 0.0f, v = 0.0f;
};

struct Indices {
    std::vector<uint16_t> indices;
};

struct MeshStats {
    bool hasValidData = false;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    float boundingBoxMin[3] = {0.0f, 0.0f, 0.0f};
    float boundingBoxMax[3] = {0.0f, 0.0f, 0.0f};
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    bool valid = false;
};

struct LoadedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    bool valid = false;
    
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

class MeshLoader {
public:
public:
    static std::vector<Vertex> extractVertices(const std::uint8_t* data, size_t offset, size_t countBytes);
    static Indices extractIndices(const std::uint8_t* data, size_t offset, size_t countBytes);
    static bool isGeometryFile(const std::string& filepath);
    static MeshStats getStats(const std::string& filepath);
    
    static void extractBoundingBox(const std::vector<Vertex>& vertices, 
                                   float& minX, float& minY, float& minZ,
                                   float& maxX, float& maxY, float& maxZ);
                                    
    static LoadedMesh load(const std::string& filepath);
    static bool loadFromDisk(LoadedMesh& mesh, const std::string& filepath);
};

} // namespace blitz

#endif // MESH_LOADER_H
