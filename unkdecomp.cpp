//
// Created by Eric on 7/06/2025.
//

#include "unkdecomp.h"

#include <filesystem>
#include <fstream>

std::vector<uint8_t> UnkDecomp::decompress(const std::vector<uint8_t>& compressedData, uint32_t decompSize) {
    _compressedData = compressedData;
    _decompData.clear();
    _decompData.resize(decompSize);

    _mem.clear();
    _mem.resize(0x10a8);

    initBitReader();

    loadDictionary();
    dumpDict("loadDictionary.bin");
    reorderDictionaryByIdx();
    dumpDict("idxSort.bin");
    polymorphicFunc();
    dumpDict("code.bin");
    reorderDictionaryByUnk();
    dumpDict("unkSort.bin");
    unkOperation();
    dumpDict("unk.bin");
    // dumpMem(0x404, _mem.size() - 0x404);
    runDecomp();

    return _decompData;
}

void UnkDecomp::initBitReader() {
    _byteBuf = _compressedData[_curPos++];
    _bitsLeft = 8;
}

void UnkDecomp::loadDictionary() {
    _unk402 = readBits16(16);
    _dictEncType = readBits8(4);
    _dictEntryNumBits = readBits8(4);


    switch (_dictEncType) {
    case 0:
        {
            for (int i = 0; i < 256; i++) {
                int value = readBits16(_dictEntryNumBits);
                _dict[_dictNumRecords].value = value;
                if (value != 0) {
                    _dict[_dictNumRecords].idx = i;
                    _dictNumRecords++;
                }
            }
        }
        break;
    case 1:
        {
            _dictNumRecords = readBits8(8);
            for (int i = 0; i < _dictNumRecords; i++) {
                _dict[i].idx = readBits8(8);
                _dict[i].value = readBits16(_dictEntryNumBits);
            }
        }
        break;
    case 2:
        {
            for (int i = 0; i < 32; i++) {
                int val = readBits8(8);
                val = val << 8;
                for (int j = 0; j < 8; j++) {
                    _dict[i * 8 + j].value = val;
                    val *= 2;
                }
            }
            for (int i = 0, j = 0; i < 256; i++) {
                if (_dict[i].value & 0x8000) {
                    _dict[j].idx = i;
                    _dict[_dictNumRecords].value = readBits16(_dictEntryNumBits);
                    _dictNumRecords++;
                    j++;
                }
            }
        }
        break;
    default:
        break;
    }

    printf("Dictionary Encoding Type: %d ValueSize: %d size: %d decompSize: %d unk402: %d\n", _dictEncType, _dictEntryNumBits, _dictNumRecords, _decompData.size(), _unk402);
}

void UnkDecomp::reorderDictionaryByIdx() {
    int k = 0;
    for (int i = _dictNumRecords - 1; i != 0; i -= k) {
        k = i;
        int idx = 0;
        for (int j = i; j != 0; j--, idx++) {
            if (_dict[idx].value <= _dict[idx + 1].value) {
                if (_dict[idx].value < _dict[idx + 1].value || _dict[idx].idx > _dict[idx + 1].idx) {
                    swapDictEntriesIdx(_dict[idx], _dict[idx + 1]);
                    k = j;
                }
            }
        }
    }
}

void UnkDecomp::reorderDictionaryByUnk() {
    int k = 0;
    for (int i = _dictNumRecords - 1; i != 0; i -= k) {
        k = i;
        int idx = 0;
        for (int j = i; j != 0; j--, idx++) {
            if (_dict[idx].unk > _dict[idx + 1].unk) {
                swapDictEntriesUnk(_dict[idx], _dict[idx + 1]);
                k = j;
            }
        }
    }
}

void UnkDecomp::swapDictEntriesIdx(DictEntry& a, DictEntry& b) {
    const int v = a.value;
    a.value = b.value;
    b.value = v;
    const int idx = a.idx;
    a.idx = b.idx;
    b.idx = idx;
}

void UnkDecomp::swapDictEntriesUnk(DictEntry& a, DictEntry& b) {
    const int v = a.value;
    a.value = b.value;
    b.value = v;
    const int unk = a.unk;
    a.unk = b.unk;
    b.unk = unk;
}

void UnkDecomp::polymorphicFunc() {
    uint16_t cx = 0;
    uint16_t dx = 0;
    uint16_t bp = _unk402;
    uint16_t di = 0x604;

    polymorphicFuncImpl(0, &di, &cx, &dx, &bp);
}

void UnkDecomp::polymorphicFuncImpl(int dictIdx, uint16_t *codePtr, uint16_t *cxUnk, uint16_t *dxUnk, uint16_t *bpUnk) {
    *bpUnk = *bpUnk - _dict[dictIdx].value;
    int curValue = _dict[dictIdx].value;
    if (*bpUnk == 0) {
        int cl = *cxUnk & 0xff;
        _dict[dictIdx].unk = cl;
        if (cl <= 8) {
            int bx = *dxUnk;
            int al = 8 - cl;
            int bl = bx & 0xff;
            bl = bl << al;
            bx = (bx & 0xff00) | bl;

            _mem[bx + 0x404] = cl;
            _dict[dictIdx].value = bx;
        } else {
            // original writes
            // mov cx, nnnn
            _mem[*codePtr] = 0xb9; // mov CX,
            *codePtr = *codePtr + 1;
            _mem[*codePtr] = cl - 8; // nnnn
            *codePtr = *codePtr + 1;

            _dict[dictIdx].value = *codePtr - 0x404; // 0x210 first value.

            *codePtr = *codePtr + 1;
            // ret
            _mem[*codePtr] = 0xc3;
            *codePtr = *codePtr + 1;
        }
    } else {
        int i = 0;
        int nextDictIdx = dictIdx;
        do {
            i += curValue;
            nextDictIdx++;
            curValue = _dict[nextDictIdx].value;
            *bpUnk = *bpUnk - _dict[nextDictIdx].value;
        } while (i <= *bpUnk);
        *bpUnk = *bpUnk + curValue;
        *cxUnk = *cxUnk + 1;
        if (*cxUnk < 9) {
            *dxUnk = *dxUnk * 2;
            polymorphicFuncImpl(nextDictIdx, codePtr, cxUnk, dxUnk, bpUnk);
            *dxUnk = *dxUnk + 1;
            *bpUnk = i;
            polymorphicFuncImpl(dictIdx, codePtr, cxUnk, dxUnk, bpUnk);
            *dxUnk = *dxUnk / 2;
        } else {
            if (*cxUnk < 10) {
                int ax = *codePtr - 0x404;
                ax = -ax;
                _mem[*dxUnk + 0x404] = ax >> 8;
                _mem[*dxUnk + 0x504] = ax & 0xff;
            }
            // shl dh, 1 code here
            _mem[*codePtr] = 0xd0;
            *codePtr = *codePtr + 1;
            _mem[*codePtr] = 0xe6;
            *codePtr = *codePtr + 1;

            // jnc +3
            _mem[*codePtr] = 0x73;
            *codePtr = *codePtr + 1;
            _mem[*codePtr] = 3;
            *codePtr = *codePtr + 1;

            // JMP
            _mem[*codePtr] = 0xe9;
            *codePtr = *codePtr + 1;
            int codeTmp = *codePtr;
            *codePtr = *codePtr + 2;
            polymorphicFuncImpl(nextDictIdx, codePtr, cxUnk, dxUnk, bpUnk);
            int ax = *codePtr - codeTmp;
            ax -= 2;
            _mem[codeTmp] = ax & 0xff;   // Possibly jmp target.
            _mem[codeTmp + 1] = ax >> 8;
            // more code created here.
            *bpUnk = i;
            polymorphicFuncImpl(dictIdx, codePtr, cxUnk, dxUnk, bpUnk);
        }
        *cxUnk = *cxUnk - 1;
    }
}

void UnkDecomp::unkOperation() {
    if (_dictNumRecords == 0) {
        return;
    }
    for (int i = 0; i < _dictNumRecords; i++) {
        int idx = _dict[i].idx;
        int value = _dict[i].value;
        if (value < 512) {
            int ah = _mem[value +  0x404];
            int cl = 1 << (8 - ah);
            for (int j = 0; j < cl; j++) {
                _mem[value + j + 0x404] = ah;
                _mem[value + j + 0x504] = idx;
            }
        } else {
            _mem[value + 0x404] = idx;
        }
    }
}

uint8_t UnkDecomp::readBits8(int numBits) {
    _byteBuf &= 0xff;
    if (_bitsLeft - numBits < 0) {
        _byteBuf = _byteBuf << _bitsLeft; //(_bitsLeft + numBits);
        numBits = numBits - _bitsLeft;
        _byteBuf |= _compressedData[_curPos++];
        _bitsLeft = 8;
    }
    _byteBuf = _byteBuf << numBits;
    _bitsLeft -= numBits;
    return _byteBuf >> 8;
}

uint16_t UnkDecomp::readBits16(int numBits) {
    uint8_t msb = 0;
    if (numBits > 8) {
        msb = readBits8(numBits - 8);
        numBits = 8;
    }
    const uint8_t lsb = readBits8(numBits);
    return msb << 8 | lsb;
}

int16_t UnkDecomp::runScript(int cx, int dh) {
    int codePtr = cx;
    uint8_t opCode = _mem[codePtr++];
    bool cflag = false;
    while (opCode != 0xc3) {
        switch (opCode) {
        case 0xd0 : // shl dh, 1
            dh <<= 1;
            if (dh & 0x100) {
                cflag = true;
            } else {
                cflag = false;
            }
            dh &= 0xff;
            codePtr++;
            break;
        case 0x73 : { // jae +3
                int offset = _mem[codePtr++];
                if (!cflag) {
                    codePtr += 3;
                }
                break;
            }
        case 0xe9 : // jmp addr
            {
                int offset = _mem[codePtr++] | _mem[codePtr++] << 8;
                codePtr += offset;
            }
            break;
        case 0xb9 : // mov CX, val
            cx = _mem[codePtr++] | _mem[codePtr++] << 8;
            break;
        default :
            printf("error @%x", codePtr);
            break;
        }
        opCode = _mem[codePtr++];
    }

    return cx;
}

void UnkDecomp::runDecomp() {
    int outIdx = 0;
    int cl = 8;
    for (int i = 0; i < _unk402; i++) {
        _bitsLeft -= cl;
        if (_bitsLeft < 0) {
            cl += _bitsLeft;
            _byteBuf = _byteBuf << cl;
            _byteBuf = (_byteBuf & 0xff00) | readCompressedByte();
            cl = -_bitsLeft;
            _bitsLeft += 8;
        }
        _byteBuf = _byteBuf << cl;
        int bx = _byteBuf >> 8;
        cl = _mem[bx + 0x404];
        if (cl & 0x80) {
            int ch = cl;
            cl = 8;
            _bitsLeft -= cl;
            if (_bitsLeft < 0) {
                cl += _bitsLeft;
                _byteBuf = _byteBuf << cl;
                _byteBuf = (_byteBuf & 0xff00) | readCompressedByte();
                cl = -_bitsLeft;
                _bitsLeft += 8;
            }
            _byteBuf = _byteBuf << cl;
            cl = _mem[bx + 0x504];
            int cx = ch << 8 | cl;
            cx = -cx;
            cx += 0x404;
            cx &= 0xffff;
            int dh = _byteBuf >> 8;
            cx = runScript(cx, dh);
            cl = cx & 0xff;
            _decompData[outIdx++] = cx >> 8;
        } else {
            _decompData[outIdx++] = _mem[bx + 0x504];
        }
    }
}

void UnkDecomp::dumpMem(int offset, int length) {
    for (int i = 0; i < length; i++) {
        printf("%02x", _mem[offset + i]);

        if (i != 0 && (i+1) % 16 == 0) {
            printf("\n");
        } else printf(" ");
    }
    std::filesystem::path path = "dump";
    path /= "mem";
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile.is_open()) {
        return;
    }
    outFile.write(reinterpret_cast<const char*>(_mem.data()) + offset, length);
    outFile.close();
}

void UnkDecomp::dumpDict(std::filesystem::path filename) {
    std::filesystem::path path = "dict";
    path /= filename;
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile.is_open()) {
        return;
    }
    for (auto entry : _dict) {
        outFile.put(entry.value & 0xff);
        outFile.put(entry.value >> 8);
        outFile.put(entry.idx);
        outFile.put(entry.unk);
    }
    outFile.close();
}

uint8_t UnkDecomp::readCompressedByte() {
    if (_curPos >= _compressedData.size()) {
        printf("Warning: Ran out of data!! at %x\n",  _curPos);
        _curPos++;
        return 0;
    }
    return  _compressedData[_curPos++];
}
