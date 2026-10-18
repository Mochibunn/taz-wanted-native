#include "pak_loader.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>

namespace blitz {

// Check if a file has valid .pc magic bytes
bool PakLoader::isValid(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    char magic[4];
    file.read(magic, 4);
    if (file.gcount() < 4) {
        return false;
    }
    
    // BlitzTech .PC magic: C6 59 60 24
    const uint8_t expected_magic[4] = {0xC6, 0x59, 0x60, 0x24};
    return (magic[0] == expected_magic[0] && 
            magic[1] == expected_magic[1] && 
            magic[2] == expected_magic[2] && 
            magic[3] == expected_magic[3]);
}

// Parse header and extract file entries from a .pc file data buffer
std::vector<FileEntry> PakLoader::parseHeader(const std::vector<uint8_t>& data) {
    std::vector<FileEntry> entries;
    
    // Verify magic bytes: C6 59 60 24
    if (data.size() < 4 || 
        data[0] != 0xC6 || data[1] != 0x59 || 
        data[2] != 0x60 || data[3] != 0x24) {
        return entries; // Not a valid .pc file
    }
    
    const uint8_t* pData = data.data();
    const uint8_t* pEnd = pData + data.size();
    
    // Start scanning after magic bytes (offset 4)
    const uint8_t* cursor = pData + 4;
    
    while (cursor < pEnd) {
        // Find null terminator for path string
        const uint8_t* nullPos = nullptr;
        size_t matchOffset = 0;
        
        for (const uint8_t* check = cursor; check < pEnd && *check != 0; ++check, ++matchOffset) {
            if (*check == 0) {
                nullPos = check;
                break;
            }
            
            // Sanity check: path should be reasonable length
            if (matchOffset > 256) {
                return entries; // Path too long, abort
            }
        }
        
        if (!nullPos) {
            // No null terminator found before end of file
            break;
        }
        
        // Extract the path string (excluding null terminator)
        const uint8_t* pathStart = cursor;
        size_t pathLength = nullPos - cursor;
        
        std::string path(reinterpret_cast<const char*>(pathStart), pathLength);
        
        // Skip trailing backslash if present (directory separator)
        if (!path.empty() && path.back() == '\\') {
            path.pop_back();
        }
        
        // Create entry for this path
        FileEntry entry;
        entry.offset = static_cast<uint64_t>(cursor - pData);
        entry.length = data.size(); // Use total size as length for now
        entry.name = path;
        entries.push_back(entry);
        
        // Move to next entry (after null terminator)
        cursor = nullPos + 1;
    }
    
    return entries;
}

// Parse a .pc file and extract all contained files
std::vector<std::string> PakLoader::parse(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    // Read entire file into memory
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    if (fileSize == 0) {
        throw std::runtime_error(filepath + ": empty file");
    }
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    if (!file.good()) {
        throw std::runtime_error("Failed to read file: " + filepath);
    }
    
    // Parse and extract file list
    auto entries = parseHeader(data);
    
    // Output results
    std::cout << "=== BlitzTech .PC Archive Reader ===" << std::endl;
    std::cout << "File: " << filepath << std::endl;
    std::cout << "Size: " << fileSize << " bytes (" << (fileSize / 1024) << " KB)" << std::endl;
    std::cout << "Found " << entries.size() << " file entries" << std::endl;
    std::cout << std::endl;
    
    // Print file list
    std::vector<std::string> fileList;
    for (const auto& entry : entries) {
        if (!entry.name.empty()) {
            fileList.push_back(entry.name);
            std::cout << "- " << entry.name << std::endl;
        }
    }
    
    return fileList;
}

} // namespace blitz
