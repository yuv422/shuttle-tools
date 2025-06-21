#include <fstream>
#include <iostream>

#include "file.h"
#include "resource.h"

void usage() {
    std::cout << "Usage: shuttle-tools <option>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << " -u <output directory> - unpack data files into output directory" << std::endl;
    std::cout << " -p <directory with unpacked files> <directory for target packed archive> <directory containing original _INDEX file> - packs all files into a new archive." << std::endl;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage();
        return 1;
    }

    if (strcmp(argv[1], "-u") == 0) {
        if (argc < 3) {
            std::cerr << "Missing output directory argument" << std::endl;
            usage();
            return 1;
        }
        std::filesystem::path unpackPath = argv[2];

        Resource res;
        if (!res.unpack(unpackPath)) {
            return 1;
        }
    } else if (strcmp(argv[1], "-p") == 0) {
        if (argc < 5) {
            std::cerr << "Missing directory arguments" << std::endl;
            usage();
            return 1;
        }
        Resource res;
        res.pack(std::filesystem::path(argv[2]),  std::filesystem::path(argv[3]), std::filesystem::path(argv[4]));
    } else {
        std::cerr << "Unknown option: " << argv[1] << std::endl;
        usage();
        return 1;
    }
    return 0;
}
