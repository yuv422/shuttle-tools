//
// Created by Eric on 19/05/2025.
//

#ifndef RESOURCE_H
#define RESOURCE_H
#include <string>
#include <vector>

#include "file.h"

struct ResFile {
    std::string name;
    int unk;
    int uncompressedSize;
    int compressedSize;
    int offset;
    int dataFileIdx;
};

class Resource {
    private:
    std::vector<ResFile> _files;
    std::vector<std::string> _dataFiles;

public:
    Resource() = default;
    bool load();
    void dumpAllFiles();

private:
    void loadDataFiles(File &index);
    void loadResFileEntries(File &index);
    static std::string parseName(File &index);
    void dumpFile(const ResFile &resFile);

    std::vector<uint8_t> *loadCompressedData(const ResFile &resFile);

    std::vector<uint8_t> decompressRLE(const ResFile &resFile, const std::vector<uint8_t> &compressedData);
};

#endif //RESOURCE_H
