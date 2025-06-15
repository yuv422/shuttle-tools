//
// Created by Eric on 7/06/2025.
//

#ifndef UNKDECOMP_H
#define UNKDECOMP_H

#include <array>
#include <filesystem>
#include <vector>

struct DictEntry {
    uint16_t value = 0;
    uint8_t idx = 0;
    uint8_t unk = 0;
};

class UnkDecomp {
private:
    std::array<DictEntry, 256> _dict = {};
    uint16_t _unk402 = 0;
    int _curPos = 0;
    uint16_t _byteBuf = 0;
    int16_t _bitsLeft = 0;
    std::vector<uint8_t> _compressedData;
    std::vector<uint8_t> _decompData;
    std::vector<uint8_t> _mem;

    int _dictEncType = 0;
    int _dictEntryNumBits = 0;
    int _dictNumRecords = 0;

public:

    std::vector<uint8_t> decompress(const std::vector<uint8_t> &compressedData, uint32_t decompSize);

private:
    void initBitReader();
    void loadDictionary();
    void reorderDictionaryByIdx();
    void reorderDictionaryByUnk();
    static void swapDictEntriesIdx(DictEntry &a, DictEntry &b);
    static void swapDictEntriesUnk(DictEntry &a, DictEntry &b);
    void polymorphicFunc();
    void polymorphicFuncImpl(int dictIdx, uint16_t *codePtr, uint16_t *cxUnk, uint16_t *dxUnk, uint16_t *bpUnk);
    void unkOperation();
    uint8_t readBits8(int numBits);
    uint16_t readBits16(int numBits);

    int16_t runScript(int cx, int dh);

    void runDecomp();
    void dumpMem(int offset, int length);
    void dumpDict(std::filesystem::path path);

    uint8_t readCompressedByte();
};
#endif //UNKDECOMP_H
