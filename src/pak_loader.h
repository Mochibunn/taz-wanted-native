#ifndef PAK_LOADER_H
#define PAK_LOADER_H

#include <string>
#include <vector>
#include <cstdint>

namespace blitz {

// BlitzTech .PC (Paks) archive parser for Taz: Wanted
class PakLoader {
public:
    // Parse a .pc file and extract all contained files
    static std::vector<std::string> parse(const std::string& filepath);

    // Check if a file appears to be a valid .pc archive
    static bool isValid(const std::string& filepath);

private:
    struct FileEntry {
        uint64_t offset;  // Offset within the .pc file where this entry starts
        size_t length;    // Length of this entry (file or padding)
        std::string name; // Null-separated path string at this offset
    };

    // Parse header and extract entries from a .pc file
    static std::vector<FileEntry> parseHeader(const std::vector<uint8_t>& data);

    // Extract null-separated path strings
    static std::vector<std::string> extractPaths(const uint8_t* data, size_t startOffset, size_t totalSize);
};

} // namespace blitz

#endif // PAK_LOADER_H
