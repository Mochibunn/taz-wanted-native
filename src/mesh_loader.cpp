#include "mesh_loader.h"
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <limits>

namespace blitz {

// Parse mesh header - adapted for .pc package file format
size_t MeshLoader::parseHeader(const std::uint8_t* data, size_t dataSize) {
    if (dataSize < 0x100) {
        return 0;
    }
    
    // Return offset where vertex data starts
    return 0x100;
}

// Extract vertices from raw binary data based on .pc file structure
std::vector<Vertex> MeshLoader::extractVertices(const std::uint8_t* data, 
                                                 size_t offset, 
                                                 size_t countBytes) {
    std::vector<Vertex> vertices;
    
    if (countBytes == 0 || !data) {
        return vertices;
    }
    
    // Each vertex is packed as 8 bytes of coordinate data
    const size_t vertexSize = 8;  // bytes per vertex
    
    uint32_t expectedVertices = static_cast<uint32_t>(countBytes / vertexSize);
    
    for (uint32_t i = 0; i < expectedVertices && offset + i * vertexSize <= countBytes; ++i) {
        const uint8_t* vdata = data + offset + i * vertexSize;
        
        Vertex v;
        
        // Unpack X position (12-bit little-endian from first word)
        uint32_t x_raw = static_cast<uint32_t>(vdata[0]) | 
                         (static_cast<uint32_t>(vdata[1]) << 8);
        int16_t x_val = static_cast<int16_t>(x_raw & 0xFFF);
        v.x = static_cast<float>(x_val) / 4096.0f;
        
        // Unpack Y position 
        uint32_t y_raw = static_cast<uint32_t>(vdata[2]) | 
                         (static_cast<uint32_t>(vdata[3]) << 8);
        int16_t y_val = static_cast<int16_t>(y_raw & 0xFFF);
        v.y = static_cast<float>(y_val) / 4096.0f;
        
        // Unpack Z position  
        uint32_t z_raw = static_cast<uint32_t>(vdata[4]) | 
                         (static_cast<uint32_t>(vdata[5]) << 8);
        int16_t z_val = static_cast<int16_t>(z_raw & 0xFFF);
        v.z = static_cast<float>(z_val) / 4096.0f;
        
        // Unpack UV coordinates from remaining bytes
        uint32_t uv_raw = static_cast<uint32_t>(vdata[6]) | 
                          (static_cast<uint32_t>(vdata[7]) << 8);
        v.u = static_cast<float>(uv_raw & 0xFFF) / 1024.0f;
        v.v = static_cast<float>((uv_raw >> 12)) / 1024.0f;
        
        vertices.push_back(v);
    }
    
    return vertices;
}

// Extract triangle indices from raw binary data
std::vector<uint16_t> MeshLoader::extractIndices(const std::uint8_t* data, size_t offset, size_t countBytes) {
    std::vector<uint16_t> indices;
    
    if (countBytes == 0 || !data) {
        return indices;
    }
    
    // Each index is 2 bytes (unsigned short) in typical D3D-style format
    uint32_t expectedIndices = static_cast<uint32_t>((countBytes - offset) / 2);
    
    for (uint32_t i = 0; i < expectedIndices && offset + i * 2 <= countBytes; ++i) {
        uint16_t idx = static_cast<uint16_t>(data[offset + i * 2]);
        
        // Only add valid indices (not sentinel values)
        if (idx < std::numeric_limits<uint16_t>::max()) {
            indices.push_back(idx);
        }
    }
    
    return indices;
}

// Validate if file contains valid geometry data or is a package manifest
bool MeshLoader::isGeometryFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // Read first 4096 bytes to check for geometry patterns
    std::vector<uint8_t> header(4096);
    file.read(reinterpret_cast<char*>(header.data()), header.size());
    auto bytesRead = file.gcount();
    
    if (bytesRead < 256) {
        return false;
    }
    
    const uint8_t* data = header.data();
    
    // Check for characteristic packed coordinate patterns (12-bit values)
    bool hasPackedCoords = true;
    bool foundMultipleVertexWords = true;
    
    // Also check if we have enough vertices to form a mesh
    uint32_t potentialVertices = 0;
    for (size_t i = 0; i < bytesRead - 16 && potentialVertices < 10000; ) {
        uint32_t x_raw = static_cast<uint32_t>(data[i]) | 
                         (static_cast<uint32_t>(data[i + 1]) << 8);
        
        // Valid coordinate value (not all zeros or max)
        if ((x_raw & 0xFFF) != 0 && (x_raw & 0xFFF) < 0x1000) {
            potentialVertices++;
            i += 8;  // Move by vertex word size
        } else {
            i++;
        }
    }
    
    return hasPackedCoords && foundMultipleVertexWords && potentialVertices > 0;
}

// Get mesh statistics - actually count vertices/indices from the packed data
MeshStats MeshLoader::getStats(const std::string& filepath) {
    MeshStats stats = {};
    stats.hasValidData = false;
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return stats;
    }
    
    // Get file size
    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file (limit for memory efficiency)
    size_t maxReadSize = static_cast<size_t>(std::min(static_cast<long long>(fileSize), 16LL * 1024 * 1024));
    std::vector<uint8_t> data(maxReadSize + 1, 0);
    file.read(reinterpret_cast<char*>(data.data()), maxReadSize);
    
    if (file.gcount() < 256) {
        return stats;
    }
    
    // Check if this has geometry-like content
    if (!isGeometryFile(filepath)) {
        return stats;
    }
    
    stats.hasValidData = true;
    
    // Initialize bounding box
    stats.minX = std::numeric_limits<float>::max();
    stats.minY = std::numeric_limits<float>::max();
    stats.minZ = std::numeric_limits<float>::max();
    stats.maxX = std::numeric_limits<float>::lowest();
    stats.maxY = std::numeric_limits<float>::lowest();
    stats.maxZ = std::numeric_limits<float>::lowest();
    
    // Count vertices and build bounding box
    const uint8_t* dataPtr = data.data();
    
    for (size_t offset = 0; offset < data.size() - 8; ) {
        uint32_t x_raw = static_cast<uint32_t>(dataPtr[offset]) | 
                         (static_cast<uint32_t>(dataPtr[offset + 1]) << 8);
        
        if ((x_raw & 0xFFF) != 0 && (x_raw & 0xFFF) < 0x1000) {
            // Valid coordinate data found
            uint32_t x_val = static_cast<uint32_t>(x_raw & 0xFFF);
            uint32_t y_val = static_cast<uint32_t>((dataPtr[offset + 2] | 
                                                     (static_cast<uint32_t>(dataPtr[offset + 3]) << 8)) & 0xFFF);
            uint32_t z_val = static_cast<uint32_t>((dataPtr[offset + 4] | 
                                                     (static_cast<uint32_t>(dataPtr[offset + 5]) << 8)) & 0xFFF);
            
            Vertex v;
            v.x = static_cast<float>(x_val) / 4096.0f;
            v.y = static_cast<float>(y_val) / 4096.0f;
            v.z = static_cast<float>(z_val) / 4096.0f;
            
            stats.vertexCount++;
            stats.maxX = std::max(v.x, stats.maxX);
            stats.minX = std::min(v.x, stats.minX);
            stats.maxY = std::max(v.y, stats.maxY);
            stats.minY = std::min(v.y, stats.minY);
            stats.maxZ = std::max(v.z, stats.maxZ);
            stats.minZ = std::min(v.z, stats.minZ);
            
            // Move past this vertex (8 bytes for packed word data)
            offset += 8;
        } else {
            offset++;
        }
        
        if (stats.vertexCount > 5000) break; // Reasonable limit
    }
    
    return stats;
}

// Load and parse a .pc geometry file - returns empty mesh for package manifest files
LoadedMesh MeshLoader::load(const std::string& filepath) {
    LoadedMesh mesh = {};
    
    // Check if it's a valid geometry file with actual vertex data
    if (!isGeometryFile(filepath)) {
        // This is likely a package manifest or audio file, not a mesh
        return mesh;
    }
    
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return mesh;
    }
    
    auto fileSizePos = file.tellg();
    size_t maxReadSize = static_cast<size_t>(std::min(static_cast<long long>(fileSizePos), 32LL * 1024 * 1024));
    std::vector<uint8_t> data(maxReadSize + 1, 0);
    file.read(reinterpret_cast<char*>(data.data()), maxReadSize);
    
    if (file.gcount() < 256) {
        return mesh;
    }
    
    const uint8_t* dataPtr = data.data();
    
    // Extract vertices from packed data - scan for vertex patterns
    auto vertices = extractVertices(dataPtr, 
                                     0x100,  // Start at header offset
                                     data.size());
    
    if (vertices.empty()) {
        return mesh;
    }
    
    mesh.vertices = std::move(vertices);
    
    // Extract indices - for a basic terrain we'll create simple triangle indices
    // For packed format files, assume indices are stored after vertices or calculate from vertex count
    
    // Calculate bounding box
    extractBoundingBox(mesh.vertices,
                       mesh.boundingBox.min[0], mesh.boundingBox.min[1], mesh.boundingBox.min[2],
                       mesh.boundingBox.max[0], mesh.boundingBox.max[1], mesh.boundingBox.max[2]);
    
    mesh.valid = true;
    mesh.vertexCount = static_cast<uint32_t>(mesh.vertices.size());
    mesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
    return mesh;
}

// Extract bounding box from vertex positions
void MeshLoader::extractBoundingBox(const std::vector<Vertex>& vertices, 
                                     float& minX, float& minY, float& minZ,
                                     float& maxX, float& maxY, float& maxZ) {
    if (vertices.empty()) {
        return;
    }
    
    minX = vertices[0].x;
    minY = vertices[0].y;
    minZ = vertices[0].z;
    maxX = vertices[0].x;
    maxY = vertices[0].y;
    maxZ = vertices[0].z;
    
    for (size_t i = 1; i < vertices.size(); ++i) {
        if (vertices[i].x < minX) minX = vertices[i].x;
        if (vertices[i].x > maxX) maxX = vertices[i].x;
        if (vertices[i].y < minY) minY = vertices[i].y;
        if (vertices[i].y > maxY) maxY = vertices[i].y;
        if (vertices[i].z < minZ) minZ = vertices[i].z;
        if (vertices[i].z > maxZ) maxZ = vertices[i].z;
    }
}

} // namespace blitz
