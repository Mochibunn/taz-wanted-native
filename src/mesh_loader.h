#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace blitz {

// BlitzTech .PC geometry parser for mesh data extraction

// Mesh vertex format based on GLS_BINARY_LSB_FIRST format
struct Vertex {
    float x, y, z;      // Position (12-bit signed each)
    float u, v;         // UV coordinates (12-bit signed each)
};

// MeshData struct - uses Vertex so it must come after Vertex definition
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    float boundingBoxMin[3];
    float boundingBoxMax[3];
    bool valid;
};

// MeshStats struct for statistics
struct MeshStats {
    uint32_t vertexCount;
    uint32_t indexCount;
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    float minU, minV, maxU, maxV;
    bool hasValidData;
};

class MeshLoader {
public:
    // Triangle index (unsigned short)
    using Indices = std::vector<uint16_t>;

    // Load and parse a .pc geometry file
    static MeshData load(const std::string& filepath);

    // Extract vertices from raw binary data
    static std::vector<Vertex> extractVertices(const std::uint8_t* data, size_t offset, size_t countBytes);

    // Extract triangle indices from raw binary data  
    static Indices extractIndices(const std::uint8_t* data, size_t offset, size_t countBytes);

    // Validate if file contains valid geometry data
    static bool isGeometryFile(const std::string& filepath);

    // Get mesh statistics
    static MeshStats getStats(const std::string& filepath);

private:
    // Packed vertex buffer format (16 bytes per vertex)
    struct VertexBuffer {
        uint32_t magic = 0x4D455348;  // "MESH" magic header
        uint32_t vertexCount = 0;
        std::array<uint8_t, 16> vertexData[1];  // Placeholder for actual data
    };

    // Parse mesh header to get vertex count and data offset
    static size_t parseHeader(const std::uint8_t* data, size_t dataSize);

    // Extract bounding box from vertex positions
    static void extractBoundingBox(const std::vector<Vertex>& vertices, float& minX, float& minY, float& minZ, 
                                   float& maxX, float& maxY, float& maxZ);

    // Convert 12-bit signed coordinates to float - declared here, defined in .cpp
    static float unpackCoord(uint32_t value);

    // Check if data offset points to actual geometry
    static bool validateGeometryOffset(const std::uint8_t* data, size_t headerSize);
};

} // namespace blitz

#endif // MESH_LOADER_H
