/* mesh_file_data.h - Mesh file data structures for terrain loading */
#ifndef MESH_FILE_DATA_H
#define MESH_FILE_DATA_H

#include <string>
#include <vector>

struct MeshFileData {
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    std::string format = "mesh_header_v1";
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    
    void parseHeader(const std::string& line) {
        if (line.find("mesh_header_v1") != std::string::npos) {
            vertex_count = 8;  // Default
        }
        
        // Extract vertex count from line
        size_t vc_start = line.find("vertex_count");
        if (vc_start != std::string::npos) {
            size_t vc_end = line.find_first_of(",}", vc_start);
            if (vc_start != std::string::npos && vc_end != std::string::npos) {
                std::string count_str = line.substr(vc_start + 12, vc_end - vc_start - 12);
                try {
                    vertex_count = static_cast<uint32_t>(std::stoi(count_str));
                } catch (...) {
                    vertex_count = 8;
                }
            }
        }
    }
};

#endif // MESH_FILE_DATA_H