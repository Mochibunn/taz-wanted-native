#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class BlitzTechFormat {
    RGB8 = 0,
    RGBA8 = 1,
    ABGR8 = 2,
    BGRA8 = 3,
    UNKNOWN = 4
};

struct BlitzTechTextureHeader {
    uint8_t magic[4];        // Magic bytes: 0xAE 0x07 0xE2 0x55
    uint32_t version;        // Version number
    uint32_t padding1;       // Padding/flags
    uint32_t data_offset;    // Offset to texture data
    uint32_t width;          // Texture width
    uint32_t height;         // Texture height  
    uint32_t depth;          // Mip levels or z-depth
    uint32_t flags;          // Format flags
    uint8_t padding2[4];     // Padding to 16 bytes per line alignment
};

struct BlitzTechTexture {
    BlitzTechTextureHeader header;
    BlitzTechFormat format;
    uint32_t width, height;
    std::vector<uint8_t> raw_data;     // Raw data in source format
    std::vector<uint8_t> converted_data;  // RGBA8 converted output (4 bytes per pixel)
    uint32_t depth;                     // Mip levels or z-depth
    bool is_valid;
};

// Parse a BlitzTech texture from file, loading data into memory and converting to RGBA8
bool load_blitztech_texture(const char* filename, BlitzTechTexture& texture);
