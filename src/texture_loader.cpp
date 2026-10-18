#include "texture_loader.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <vector>

// Texture format constants
constexpr uint8_t BLITZTEX_MAGIC[4] = {0xAE, 0x07, 0xE2, 0x55};
constexpr size_t HEADER_SIZE = sizeof(BlitzTechTextureHeader);

// Get texture format based on flags field
BlitzTechFormat get_blitztech_format(BlitzTechTextureHeader& header) {
    // Check all magic bytes
    if (header.magic[0] != BLITZTEX_MAGIC[0] || 
        header.magic[1] != BLITZTEX_MAGIC[1] ||
        header.magic[2] != BLITZTEX_MAGIC[2] ||
        header.magic[3] != BLITZTEX_MAGIC[3]) {
        return BlitzTechFormat::UNKNOWN;
    }

    // For now, default to RGBA8 when magic is valid
    return BlitzTechFormat::RGBA8;
}

// Convert raw pixel data to RGBA8 format (normalized order: B,G,R,A)
void convert_pixel_to_rgba(const uint8_t* src_pixel, BlitzTechFormat format,
                           uint8_t* dst_rgba) {
    switch (format) {
        case BlitzTechFormat::RGB8:
            // Source is RGB order
            dst_rgba[0] = src_pixel[0];   // B component stored as first byte
            dst_rgba[1] = src_pixel[1];   // G
            dst_rgba[2] = src_pixel[2];   // R
            dst_rgba[3] = 255;            // Assume opaque alpha
            break;
        case BlitzTechFormat::RGBA8:
            // Source is already RGBA order, convert to B,G,R,A for standard RGBA8
            dst_rgba[0] = src_pixel[0];   // First byte (often R in BLITZTEX)
            dst_rgba[1] = src_pixel[1];
            dst_rgba[2] = src_pixel[2];
            dst_rgba[3] = src_pixel[3];   // Alpha
            break;
        case BlitzTechFormat::ABGR8:
            // Source is ABGR order (A,B,G,R)
            dst_rgba[0] = src_pixel[1];   // B from position 1
            dst_rgba[1] = src_pixel[2];   // G
            dst_rgba[2] = src_pixel[3];   // R
            dst_rgba[3] = src_pixel[0];   // A
            break;
        case BlitzTechFormat::BGRA8:
            // Source is BGRA order (B,G,R,A) - most common
            dst_rgba[0] = src_pixel[0];   // B
            dst_rgba[1] = src_pixel[1];   // G
            dst_rgba[2] = src_pixel[2];   // R
            dst_rgba[3] = src_pixel[3];   // A
            break;
        default:
            // Unknown format - zero all channels
            dst_rgba[0] = 0;
            dst_rgba[1] = 0;
            dst_rgba[2] = 0;
            dst_rgba[3] = 0;
    }
}

// Load and convert a BlitzTech texture to RGBA8 format
bool load_blitztech_texture(const char* filename, BlitzTechTexture& texture) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open texture file: " << filename << std::endl;
        return false;
    }

    // Read the complete header (16 bytes)
    file.read(reinterpret_cast<char*>(&texture.header), sizeof(BlitzTechTextureHeader));
    
    if (texture.header.magic[0] != BLITZTEX_MAGIC[0] ||
        texture.header.magic[1] != BLITZTEX_MAGIC[1] ||
        texture.header.magic[2] != BLITZTEX_MAGIC[2] ||
        texture.header.magic[3] != BLITZTEX_MAGIC[3]) {
        std::cerr << "Invalid BlitzTech texture magic bytes" << std::endl;
        return false;
    }

    // Determine texture format based on valid magic bytes
    texture.format = get_blitztech_format(texture.header);
    
    // Seek to the actual texture data
    file.seekg(texture.header.data_offset);
    
    if (!file.good()) {
        std::cerr << "Failed to seek to texture data at offset: " 
                   << texture.header.data_offset << std::endl;
        return false;
    }

    // Calculate texture dimensions from header fields
    uint32_t width = texture.header.width;
    uint32_t height = texture.header.height;
    
    // Use the depth field from the header (mip levels or z-depth)
    texture.depth = texture.header.depth;  // Access through header structure
    
    // Allocate buffer for raw texture data
    std::vector<uint8_t> raw_data(width * height * 4);
    if (!file.read(reinterpret_cast<char*>(raw_data.data()), width * height * 4)) {
        std::cerr << "Failed to read texture data: " << filename << std::endl;
        return false;
    }
    
    // Store loaded texture metadata
    texture.width = width;
    texture.height = height;
    texture.raw_data = raw_data;  // Raw data in source format
    texture.is_valid = true;
    
    std::cout << "Loaded BlitzTech texture: "
              << "file=" << filename
              << ", size=" << width << "x" << height
              << ", format=" << static_cast<int>(texture.format)
              << std::endl;
    
    // Convert to RGBA8 format if needed
    bool conversion_needed = (texture.format != BlitzTechFormat::RGBA8);
    
    if (conversion_needed || raw_data.empty()) {
        // Use zero-initialized buffer
        texture.converted_data.resize(raw_data.size(), 0);
        uint32_t num_pixels = width * height;
        
        for (uint32_t i = 0; i < num_pixels; ++i) {
            const uint8_t* src_pixel = raw_data.data() + (i * 4);
            convert_pixel_to_rgba(src_pixel, texture.format, 
                                 texture.converted_data.data() + (i * 4));
        }
    } else {
        // Data is already in correct RGBA order
        texture.converted_data = std::move(raw_data);
    }
    
    return true;
}
