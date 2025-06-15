#include <fstream>
#include <iostream>

#include "file.h"
#include "resource.h"

int main(int argc, char *argv[]) {
    Resource res;
    if (!res.load()) {
        return 1;
    }
    return 0;
}
