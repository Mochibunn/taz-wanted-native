#include "mesh_loader.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <.pc file path>\n";
        return 1;
    }
    
    const std::string filepath = argv[1];
    
    std::cout << "Testing mesh loader with file: " << filepath << "\n";
    std::cout << "================================\n\n";
    
    // Test 1: Check if file is a geometry file
    std::cout << "Test 1: File type detection\n";
    bool isGeo = blitz::MeshLoader::isGeometryFile(filepath);
    std::cout << "  Is geometry file: " << (isGeo ? "YES" : "NO") << "\n";
    
    if (!isGeo) {
        std::cerr << "  ERROR: File does not appear to contain valid geometry!\n\n";
        return 1;
    }
    
    // Test 2: Get mesh statistics
    std::cout << "\nTest 2: Mesh statistics\n";
    auto stats = blitz::MeshLoader::getStats(filepath);
    
    std::cout << "  Vertex count: " << stats.vertexCount << "\n";
    std::cout << "  Index count: " << stats.indexCount << "\n";
    std::cout << "  Bounding box min: (" 
              << stats.boundingBoxMin[0] << ", "
              << stats.boundingBoxMin[1] << ", "
              << stats.boundingBoxMin[2] << ")\n";
    std::cout << "  Bounding box max: ("
              << stats.boundingBoxMax[0] << ", "
              << stats.boundingBoxMax[1] << ", "
              << stats.boundingBoxMax[2] << ")\n";
    std::cout << "  Has valid data: " 
              << (stats.hasValidData ? "YES" : "NO") << "\n\n";
    
    if (!stats.hasValidData) {
        std::cerr << "  ERROR: No valid geometry data found!\n\n";
        return 1;
    }
    
    // Test 3: Load full mesh
    std::cout << "Test 3: Full mesh loading\n";
    auto mesh = blitz::MeshLoader::load(filepath);
    
    if (mesh.valid) {
        std::cout << "  Mesh loaded successfully!\n";
        std::cout << "  Vertices extracted: " << mesh.vertices.size() << "\n";
        std::cout << "  Indices extracted: " << mesh.indices.size() << "\n";
        
        // Display first few vertices
        std::cout << "\n  First 5 vertices:\n";
        for (size_t i = 0; i < std::min(static_cast<size_t>(5), mesh.vertices.size()); ++i) {
            const auto& v = mesh.vertices[i];
            std::cout << "    Vertex " << i << ": ("
                      << std::fixed << std::setprecision(4) 
                      << v.x << ", " << v.y << ", " << v.z << ") UV=(" << v.u << ", " << v.v << ")\n";
        }
        
        std::cout << "\n  Bounding box: ("
                  << mesh.boundingBoxMin[0] << ", "
                  << mesh.boundingBoxMin[1] << ", "
                  << mesh.boundingBoxMin[2] << ") - ("
                  << mesh.boundingBoxMax[0] << ", "
                  << mesh.boundingBoxMax[1] << ", "
                  << mesh.boundingBoxMax[2] << ")\n";
    } else {
        std::cerr << "  ERROR: Failed to load mesh from file!\n\n";
        return 1;
    }
    
    // Test 4: Write extracted data to a debug file
    std::cout << "\nTest 4: Writing extracted data\n";
    try {
        std::ofstream out("mesh_debug.bin", std::ios::binary);
        if (out.is_open()) {
            // Write vertex count
            uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
            out.write(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
            
            // Write vertices
            for (const auto& v : mesh.vertices) {
                float verts[4] = {v.x, v.y, v.z, 0.0f};  // x, y, z, w
                out.write(reinterpret_cast<char*>(verts), sizeof(verts));
                
                float uvs[2] = {v.u, v.v};  // u, v
                out.write(reinterpret_cast<char*>(uvs), sizeof(uvs));
            }
            
            // Write index count
            uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
            out.write(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
            
            // Write indices
            for (uint16_t idx : mesh.indices) {
                out.write(reinterpret_cast<char*>(&idx), sizeof(idx));
            }
            
            out.close();
            std::cout << "  Written mesh data to mesh_debug.bin\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "  ERROR writing debug file: " << e.what() << "\n";
    }
    
    std::cout << "\n================================\n";
    std::cout << "All tests completed successfully!\n";
    std::cout << "Mesh extraction for .pc files is working.\n";
    
    return 0;
}
