//
// Created by Eric on 19/05/2025.
//

#ifndef RESOURCE_H
#define RESOURCE_H
#include <filesystem>
#include <string>
#include <vector>

#include "file.h"

enum EncodingType {
    UNCOMPRESSED = 0,
    RLE = 1,
    DICTIONARY = 2,
    RLE_AND_DICTIONARY = 3,
};

struct ResFile {
    std::string name;
    EncodingType encodingType = UNCOMPRESSED;
    int uncompressedSize;
    int compressedSize;
    int offset;
    int dataFileIdx;
};

class Resource {
    private:
    std::vector<ResFile> _files;
    std::vector<std::string> _dataFiles;
    std::filesystem::path _unpackPath;
    std::filesystem::path _packedPath;

public:
    Resource() = default;
    bool unpack(std::filesystem::path path = "dump");
    void dumpAllFiles();
    bool pack(const std::filesystem::path& unpackedPath, const std::filesystem::path& packedPath, const std::filesystem::path& originalIndexPath);

private:
    void loadDataFiles(File &index);
    void loadResFileEntries(File &index);
    static std::string parseName(File &index);
    void dumpFile(const ResFile &resFile);

    std::vector<uint8_t> *loadCompressedData(const std::filesystem::path &dataFilePath,  const ResFile &resFile);

    std::vector<uint8_t> decompressRLE(const std::vector<uint8_t> &compressedData);

    bool writeIndexFile();
    void addFileToDataFile(int dataFileIdx, const std::filesystem::path& inFilePath);

    std::vector<std::string> loadSortedNamesFromIndex(const std::filesystem::path& originalIndexPath);
};

#endif //RESOURCE_H
