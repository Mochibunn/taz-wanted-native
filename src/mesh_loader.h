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

struct BoundingBox {
    float min[3] = {0.0f, 0.0f, 0.0f};
    float max[3] = {0.0f, 0.0f, 0.0f};
};

struct MeshStats {
    bool hasValidData = false;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
};

struct LoadedMesh {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    bool valid = false;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    BoundingBox boundingBox;
};

class MeshLoader {
public:
    static size_t parseHeader(const std::uint8_t* data, size_t dataSize);
    static std::vector<Vertex> extractVertices(const std::uint8_t* data, size_t offset, size_t countBytes);
    static std::vector<uint16_t> extractIndices(const std::uint8_t* data, size_t offset, size_t countBytes);
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
