//
// Created by Eric on 19/05/2025.
//

#include "file.h"

File::File(const std::string& filename) {
    file.open(filename, std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
    }
}

void File::seekg(std::streamoff off, std::ios_base::seekdir way) {
    file.seekg(off, way);
}

uint8_t File::readUInt8() {
    uint8_t byte;
    file.read((char*)&byte, 1);
    return byte;
}

uint16_t File::readUInt16() {
    uint8_t lsb, msb;
    file.read(reinterpret_cast<char*>(&lsb), 1);
    file.read(reinterpret_cast<char*>(&msb), 1);

    return (uint16_t)((msb << 8) | lsb);
}

uint32_t File::readUInt32() {
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
    uint8_t b4;
    file.read(reinterpret_cast<char*>(&b1), 1);
    file.read(reinterpret_cast<char*>(&b2), 1);
    file.read(reinterpret_cast<char*>(&b3), 1);
    file.read(reinterpret_cast<char*>(&b4), 1);

    return (uint32_t)((b4 << 24) | (b3 << 16) | (b2 << 8) | b1);
}

void File::readBytes(char* buffer, uint32_t size) {
    file.read(buffer, size);
}
