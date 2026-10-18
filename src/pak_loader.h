#ifndef PAK_LOADER_H
#define PAK_LOADER_H

#include <string>
#include <vector>
#include <cstdint>

namespace blitz {

struct FileEntry {
    uint64_t offset;
    uint64_t length;
    std::string name;
};

class PakLoader {
public:
    static bool isValid(const std::string& filepath);
    static std::vector<FileEntry> parseHeader(const std::vector<uint8_t>& data);
    static std::vector<std::string> parse(const std::string& filepath);
};

} // namespace blitz

#endif // PAK_LOADER_H
