#ifndef TEXTURE_LOADER_H
#define TEXTURE_LOADER_H

#include <cstdint>
#include <string>
#include <vector>

namespace blitz {

enum class BlitzTechFormat {
    UNKNOWN = 0,
    RGB8,
    RGBA8,
    ABGR8,
    BGRA8
};

struct BlitzTechTextureHeader {
    uint8_t magic[4] = {};
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t depth = 1;
    uint32_t data_offset = 0;
    uint32_t flags = 0;
};

struct BlitzTechTexture {
    BlitzTechFormat format = BlitzTechFormat::UNKNOWN;
    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t depth = 1;
    std::vector<uint8_t> raw_data;
    std::vector<uint8_t> converted_data;
    bool is_valid = false;
};

} // namespace blitz

#endif // TEXTURE_LOADER_H
