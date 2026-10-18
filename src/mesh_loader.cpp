#include "mesh_loader.h"
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>
#include <limits>

namespace blitz {

// Convert 12-bit signed value to float (little-endian packed format)
float unpackCoord(uint32_t value) {
    // Mask to get lower 12 bits
    int16_t val = static_cast<int16_t>(value & 0xFFF);
    
    // Scale from -4096 to 4095 range to normalized float
    // Using a reasonable scale factor for world coordinates
    float scaled = (val + 4096.0f) / 4096.0f;  // Normalize to [0, 1] then can be scaled
    
    return static_cast<float>(scaled);
}

// Parse mesh header - adapted for .pc package file format
size_t MeshLoader::parseHeader(const std::uint8_t* data, size_t dataSize) {
    if (dataSize < 0x200) {
        return 0;
    }
    
    // .pc files appear to have metadata at the start
    // Skip for vertex extraction
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
    
    // Each vertex is 16 bytes based on binary analysis:
    // Packed 12-bit coordinates per component
    
    const size_t vertexSize = 16;  // bytes per vertex
    uint32_t expectedVertices = static_cast<uint32_t>(countBytes / vertexSize);
    
    for (uint32_t i = 0; i < expectedVertices && offset + i * vertexSize <= countBytes; ++i) {
        const uint8_t* vdata = data + offset + i * vertexSize;
        
        Vertex v;
        
        // Unpack X position (12-bit little-endian from first word)
        uint32_t x_raw = *reinterpret_cast<const uint16_t*>(vdata);
        uint32_t x_val = x_raw & 0xFFF;
        v.x = static_cast<float>(x_val) / 4096.0f;
        
        // Unpack Y position 
        x_raw = *reinterpret_cast<const uint16_t*>(vdata + 2);
        uint32_t y_val = x_raw & 0xFFF;
        v.y = static_cast<float>(y_val) / 4096.0f;
        
        // Unpack Z position  
        x_raw = *reinterpret_cast<const uint16_t*>(vdata + 4);
        uint32_t z_val = x_raw & 0xFFF;
        v.z = static_cast<float>(z_val) / 4096.0f;
        
        // Unpack U UV coordinate (if present in this packed format)
        x_raw = *reinterpret_cast<const uint16_t*>(vdata + 6);
        uint32_t uv_word = x_raw & 0xFFF;
        v.u = static_cast<float>(uv_word % 65536) / 4096.0f;
        
        // Unpack V UV coordinate (upper bits or separate word)
        x_raw = *reinterpret_cast<const uint16_t*>(vdata + 8);
        v.v = static_cast<float>((x_raw & 0xFFF)) / 4096.0f;
        
        vertices.push_back(v);
    }
    
    return vertices;
}

// Extract triangle indices from raw binary data
MeshLoader::Indices MeshLoader::extractIndices(const std::uint8_t* data, 
                                                size_t offset, 
                                                size_t countBytes) {
    Indices indices;
    
    if (countBytes == 0 || !data) {
        return indices;
    }
    
    // Each index is 2 bytes (unsigned short) in typical D3D-style format
    uint32_t expectedIndices = static_cast<uint32_t>(countBytes / 2);
    
    for (uint32_t i = 0; i < expectedIndices && offset + i * 2 <= countBytes; ++i) {
        uint16_t idx = *reinterpret_cast<const uint16_t*>(data + offset + i * 2);
        
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
    // These files have repeated LSB patterns typical of vertex buffers
    bool hasPackedCoords = false;
    bool foundMultipleVertexWords = false;
    
    for (size_t i = 0; i < bytesRead - 3 && !foundMultipleVertexWords; ++i) {
        if ((data[i] & 0x80) == 0 && i < bytesRead - 4) {
            hasPackedCoords = true;
            
            // Check for vertex word pattern (multiple valid LSB-aligned values)
            uint32_t val1 = *reinterpret_cast<const uint32_t*>(data + i);
            uint32_t val2 = *reinterpret_cast<const uint32_t*>(data + i + 8);
            
            if ((val1 & 0xF000) == 0 && (val2 & 0xF000) == 0) {
                foundMultipleVertexWords = true;
            }
        }
    }
    
    // Also check if we have enough vertices to form a mesh
    uint32_t potentialVertices = 0;
    for (size_t i = 0; i < bytesRead - 16 && potentialVertices < 10000; ) {
        uint32_t x_raw = *reinterpret_cast<const uint16_t*>(data + i);
        
        // Valid coordinate value (not all zeros or max)
        if ((x_raw & 0xFFF) != 0 && (x_raw & 0xFFF) < 0x1000) {
            potentialVertices++;
            i += 8;  // Move by vertex word size
        } else {
            i++;
        }
    }
    
    return hasPackedCoords && foundMultipleVertexWords;
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
    auto fileSizePos = file.tellg();
    std::streampos fileSizeStream = fileSizePos;
    size_t maxReadSize = static_cast<size_t>(std::min(static_cast<long long>(fileSizeStream), 16LL * 1024 * 1024));
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
    
    for (size_t offset = 0; offset < data.size() - 16; ) {
        uint32_t x_raw = *reinterpret_cast<const uint16_t*>(dataPtr + offset);
        
        if ((x_raw & 0xFFF) != 0 && (x_raw & 0xFFF) < 0x1000) {
            // Valid coordinate data found
            uint32_t x_val = (x_raw & 0xFFF);
            uint32_t y_val = (*reinterpret_cast<const uint16_t*>(dataPtr + offset + 2)) & 0xFFF;
            uint32_t z_val = (*reinterpret_cast<const uint16_t*>(dataPtr + offset + 4)) & 0xFFF;
            
            Vertex v;
            v.x = static_cast<float>(x_val) / 4096.0f;
            v.y = static_cast<float>(y_val) / 4096.0f;
            v.z = static_cast<float>(z_val) / 4096.0f;
            
            stats.vertexCount++;
            
            // Update bounding box
            if (v.x < stats.minX) stats.minX = v.x;
            if (v.x > stats.maxX) stats.maxX = v.x;
            if (v.y < stats.minY) stats.minY = v.y;
            if (v.y > stats.maxY) stats.maxY = v.y;
            if (v.z < stats.minZ) stats.minZ = v.z;
            if (v.z > stats.maxZ) stats.maxZ = v.z;
            
            // Move past this vertex (skip 8 bytes for packed word data)
            offset += 8;
        } else {
            offset++;
        }
        
        if (stats.vertexCount > 5000) break; // Reasonable limit
    }
    
    return stats;
}

// Load and parse a .pc geometry file - returns empty mesh for package manifest files
MeshData MeshLoader::load(const std::string& filepath) {
    MeshData mesh = {};
    
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
                                     0,  // Start at beginning after initial header check
                                     data.size());
    
    if (vertices.empty()) {
        return mesh;
    }
    
    mesh.vertices = std::move(vertices);
    
    // Extract indices if present in the data structure
    // In .pc files, indices may be interleaved or in a separate section
    auto indices = extractIndices(dataPtr, 0, data.size());
    
    if (!indices.empty()) {
        mesh.indices = std::move(indices);
    }
    
    // Calculate bounding box from vertices
    extractBoundingBox(mesh.vertices,
                       mesh.boundingBoxMin[0], mesh.boundingBoxMin[1], mesh.boundingBoxMin[2],
                       mesh.boundingBoxMax[0], mesh.boundingBoxMax[1], mesh.boundingBoxMax[2]);
    
    mesh.valid = true;
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
