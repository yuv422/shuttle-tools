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

bool Resource::unpack(std::filesystem::path path) {
    auto index = File("_INDEX");
    if (!index.isOpen()) {
        std::cerr  << "Failed to open _INDEX file" << std::endl;
        return false;
    }

    _unpackPath = path;

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

bool Resource::pack(const std::filesystem::path& unpackedPath, const std::filesystem::path& packedPath, const std::filesystem::path& originalIndexPath) {
    auto sortedNames = loadSortedNamesFromIndex(originalIndexPath);

    _files.clear();
    _dataFiles.clear();
    _unpackPath = unpackedPath;
    _packedPath = packedPath;

    std::filesystem::create_directory(_packedPath);

    std::filesystem::path dataFilePath = _packedPath / "_data1";
    std::remove(dataFilePath.string().c_str());
    _dataFiles.push_back(dataFilePath.string());

    for (auto const &name : sortedNames) {
        auto file = _unpackPath / name;
        addFileToDataFile(0, file);
    }

    writeIndexFile();
    return true;
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
        file.encodingType = static_cast<EncodingType>(index.readUInt8());
        file.offset = index.readUInt32();
        file.compressedSize = index.readUInt32();
        file.uncompressedSize = index.readUInt32();
        _files[i] = file;
        printf("%s: idx=%d, encodingType = %d, off=%d, comp=%d, uncomp=%d\n",
            file.name.c_str(), file.dataFileIdx, file.encodingType, file.offset, file.compressedSize, file.uncompressedSize);
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
    std::filesystem::path path = _unpackPath;
    std::filesystem::create_directory(path);

    path /= resFile.name;
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile.is_open()) {
        std::cerr  << "Failed to open " << path << " file" << std::endl;
        return;
    }

    printf("dumping %s encType: %d\n", resFile.name.c_str(), resFile.encodingType);

    auto compressedData = loadCompressedData(_dataFiles[resFile.dataFileIdx - 1], resFile);
    if (!compressedData) {
        outFile.close();
        return;
    }

    auto unkDecomp = UnkDecomp();
    switch (resFile.encodingType) {
    case 0 :
        outFile.write(reinterpret_cast<const char*>(compressedData->data()), resFile.compressedSize);
        break;
    case 1 : {
            auto decompressedData = decompressRLE(*compressedData);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
            break;
        }
    case 2 : {
            auto decompressedData = unkDecomp.decompress(*compressedData, resFile.uncompressedSize);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), resFile.uncompressedSize);
            break;
        }
    case 3 : {
            auto rleData = unkDecomp.decompress(*compressedData, resFile.uncompressedSize);
            // outFile.write(reinterpret_cast<const char*>(rleData.data()), resFile.uncompressedSize);
            auto decompressedData = decompressRLE(rleData);
            outFile.write(reinterpret_cast<const char*>(decompressedData.data()), decompressedData.size());
            break;
        }
    }
    outFile.close();

    delete compressedData;
}

std::vector<uint8_t> *Resource::loadCompressedData(const std::filesystem::path &dataFilePath, const ResFile& resFile) {
    auto data = File(dataFilePath.string());
    if (!data.isOpen()) {
        std::cerr  << "Failed to open archive " << dataFilePath << " file" << std::endl;
        return nullptr;
    }
    data.seekg(resFile.offset, std::ios::beg);

    auto buf = new char[resFile.compressedSize];
    data.readBytes(buf, resFile.compressedSize);

    auto vec = new std::vector<uint8_t>();
    vec->assign(buf, buf + resFile.compressedSize);
    return vec;
}

std::vector<uint8_t> Resource::decompressRLE(const std::vector<uint8_t>& compressedData) {
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

void write4Bytes(std::ofstream &file, int value) {
    file.put(value & 0xff);
    file.put((value >> 8) & 0xff);
    file.put((value >> 16) & 0xff);
    file.put((value >> 24) & 0xff);
}

void write2Bytes(std::ofstream &file, int value) {
    file.put(value & 0xff);
    file.put((value >> 8) & 0xff);
}

bool Resource::writeIndexFile() {
    std::ofstream indexFile;
    std::filesystem::path path = _packedPath / "_INDEX";

    indexFile.open(path, std::ios::binary);
    indexFile.put('I');
    indexFile.put('n');
    indexFile.put('d');
    indexFile.put('x');

    write4Bytes(indexFile, 0xc); // header size
    write4Bytes(indexFile, 16 + _dataFiles.size() * 0x50); // offset to start of file records
    write2Bytes(indexFile, _dataFiles.size()); // number of data archive container files
    write2Bytes(indexFile, 1); // unknown value

    for (auto pathString : _dataFiles) { // output data archive filename.
        auto archiveFileName = std::filesystem::path(pathString).filename().string();
        for (auto chr : archiveFileName) {
            indexFile.put(chr);
        }
        for (int i = 0; i < 0x50 - archiveFileName.size(); i++) {
            indexFile.put(0);
        }
    }

    write2Bytes(indexFile, _files.size()); // number of files added to the resource
    write2Bytes(indexFile, 1); // unknown value

    for (auto resFile : _files) {
        for (auto chr : resFile.name) {
            indexFile.put(chr);
        }
        for (int i = 0; i < 12 -  resFile.name.size(); i++) {
            indexFile.put(' ');
        }
        indexFile.put(resFile.dataFileIdx);
        indexFile.put(resFile.encodingType);
        write4Bytes(indexFile, resFile.offset);
        write4Bytes(indexFile, resFile.compressedSize);
        write4Bytes(indexFile, resFile.uncompressedSize);
    }

    indexFile.flush();
    indexFile.close();

    return true;
}

void Resource::addFileToDataFile(int dataFileIdx, const std::filesystem::path& inFilePath) {
    std::ofstream dataFile;

    dataFile.open(_dataFiles[dataFileIdx], std::ios::binary | std::ios_base::app);

    auto inFile = File(inFilePath.string());
    if (!inFile.isOpen()) {
        std::cerr  << "Failed to open unpacked " << inFilePath << " file" << std::endl;
        return;
    }

    dataFile.seekp(0, std::ios::end);
    int dataOffset = dataFile.tellp();

    ResFile resFile;
    resFile.name = inFilePath.filename().string();
    resFile.encodingType = UNCOMPRESSED;
    resFile.compressedSize = inFile.size();
    resFile.uncompressedSize = inFile.size();
    resFile.dataFileIdx = dataFileIdx + 1;
    resFile.offset = dataOffset;

    _files.push_back(resFile);

    auto buf = new char[resFile.uncompressedSize];
    inFile.readBytes(buf, resFile.uncompressedSize);

    dataFile.write(buf, resFile.uncompressedSize);
    dataFile.close();
}

std::vector<std::string> Resource::loadSortedNamesFromIndex(const std::filesystem::path& originalIndexPath) {
    auto originalIndexFile = originalIndexPath / "_INDEX";
    auto originalIndex = File(originalIndexFile.string());
    if (!originalIndex.isOpen()) {
        std::cerr  << "Failed to open original _INDEX file" << std::endl;
        return {};
    }

    loadResFileEntries(originalIndex);

    std::vector<std::string> sortedNames(_files.size());

    std::transform(
        _files.begin(), _files.end(), sortedNames.begin(),
        [](const ResFile& in) {
            return in.name;
        }
    );
    return sortedNames;
}
