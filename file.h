//
// Created by Eric on 19/05/2025.
//

#ifndef FILE_H
#define FILE_H

#include <fstream>

class File {
private:
    std::ifstream file;
    uint32_t fileSize = 0;

public:
    File(const std::string &filename);

    bool isOpen() const { return file.is_open(); }

    uint32_t size() const { return fileSize; }
    void seekg (std::streamoff off, std::ios_base::seekdir way);

    uint8_t readUInt8();
    uint16_t readUInt16();
    uint32_t readUInt32();
    void readBytes(char *buffer, uint32_t size);
};

#endif //FILE_H
