//
// Created by Eric on 19/05/2025.
//

#include "resource.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

#include "file.h"
#include "unkdecomp.h"

std::string str_toupper(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}

bool Resource::load() {
    auto index = File("_INDEX");
    if (!index.isOpen()) {
        return false;
    }

    loadDataFiles(index);
    loadResFileEntries(index);
    dumpAllFiles();

    return true;
}

void Resource::dumpAllFiles() {
    for (auto &file : _files) {
        dumpFile(file);
    }
}

void Resource::loadDataFiles(File& index) {
    index.seekg(12, std::ios::beg);
    auto numFiles = index.readUInt16();
    index.seekg(16, std::ios::beg);
    _dataFiles.resize(numFiles);
    for (int i = 0; i < numFiles; i++) {
        char buf[0x50];
        index.readBytes(buf, 0x50);
        _dataFiles[i] = str_toupper(buf);
    }
}

void Resource::loadResFileEntries(File& index) {
    index.seekg(8, std::ios::beg);
    auto fileOffset = index.readUInt16();

    auto numFiles = (index.size() - fileOffset) / 0x1a;

    _files.resize(numFiles);

    index.seekg(fileOffset + 4, std::ios::beg);

    std::cout << "numFiles = " << numFiles << std::endl;

    for (auto i = 0; i < numFiles; i++) {
        ResFile file;
        file.name = parseName(index);
        file.dataFileIdx = index.readUInt8();
        file.unk = index.readUInt8();
        file.offset = index.readUInt32();
        file.compressedSize = index.readUInt32();
        file.uncompressedSize = index.readUInt32();
        _files[i] = file;
        // index.seekg(10, std::ios::cur);
        printf("%s: idx=%d, unk = %d, off=%d, comp=%d, uncomp=%d\n",
            file.name.c_str(), file.dataFileIdx, file.unk, file.offset, file.compressedSize, file.uncompressedSize);
    }
}


std::string Resource::parseName(File& index) {
    char name[13];
    index.readBytes(name, 12);

    for (int i = 0; i < 12; i++) {
        if (name[i] == ' ') {
            name[i] = '\0';
        }
    }
    name[12] = '\0';
    return name;
}

void Resource::dumpFile(const ResFile& resFile) {
    std::filesystem::path path = "dump";
    path /= resFile.name;
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile.is_open()) {
        return;
    }

    auto compressedData = loadCompressedData(resFile);
    if (!compressedData) {
        outFile.close();
        return;
    }

    printf("dumping %s encType: %d\n", resFile.name.c_str(), resFile.unk);
    auto unkDecomp = UnkDecomp();
    switch (resFile.unk) {
    case 0 :
        outFile.write(reinterpret_cast<const char*>(compressedData->data()), resFile.compressedSize);
        break;
    case 1 : {
            auto decompressedData = decompressRLE(resFile, *compressedData);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
            break;
        }
    case 2 : {
            auto decompressedData = unkDecomp.decompress(*compressedData, resFile.uncompressedSize);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), resFile.uncompressedSize);
            break;
        }
    case 3 :
        {
            auto rleData = unkDecomp.decompress(*compressedData, resFile.uncompressedSize);
            auto decompressedData = decompressRLE(resFile, rleData);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
            break;
        }
    }
    outFile.close();

    delete compressedData;
}

std::vector<uint8_t> *Resource::loadCompressedData(const ResFile& resFile) {
    auto data = File(_dataFiles[resFile.dataFileIdx - 1]);
    if (!data.isOpen()) {
        return nullptr;
    }
    data.seekg(resFile.offset, std::ios::beg);

    auto buf = new char[resFile.compressedSize];
    data.readBytes(buf, resFile.compressedSize);

    auto vec = new std::vector<uint8_t>();
    vec->assign(buf, buf + resFile.compressedSize);
    return vec;
}

std::vector<uint8_t> Resource::decompressRLE(const ResFile& resFile, const std::vector<uint8_t>& compressedData) {
    int numBytesLeftToWrite = compressedData[1] << 8 | compressedData[0];
    auto uncompressedData = std::vector<uint8_t>();
    uncompressedData.resize(numBytesLeftToWrite);
    int outIdx = 0;

    for (int i = 2; i < compressedData.size() && numBytesLeftToWrite > 0;) {
        auto controlByte = compressedData[i++];
        if (controlByte & 0x80) {
            uint16_t count = 0;
            if (controlByte & 0x40) {
                count = (controlByte & 0x3f) << 8 | compressedData[i++];
                count += 0x44;
            } else {
                count = controlByte & 0x3f;
                count += 4;
            }
            auto dataByte = compressedData[i++];
            for (int j = 0; j < count; j++) {
                uncompressedData[outIdx++] = dataByte;
            }
            numBytesLeftToWrite -= count;
        } else {
            if (controlByte & 0x40) {
                uint16_t count = (controlByte & 0x3f) << 8 | compressedData[i++];
                count += 0x41;
                for (int j = 0; j < count; j++) {
                    uncompressedData[outIdx++] = compressedData[i++];
                }
                numBytesLeftToWrite -= count;
            } else {
                auto dataByte = compressedData[i++];
                auto count = controlByte;
                for (int j = 0; j < count; j++) {
                    uncompressedData[outIdx++] = compressedData[i++];
                }
                uncompressedData[outIdx++] = dataByte;
                numBytesLeftToWrite -= (count + 1);
            }
        }
    }
    return uncompressedData;
}
