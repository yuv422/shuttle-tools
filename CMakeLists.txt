cmake_minimum_required(VERSION 3.30)
project(shuttle-tools)

set(CMAKE_CXX_STANDARD 20)

add_executable(shuttle-tools main.cpp
        file.cpp
        file.h
        resource.cpp
        resource.h
        unkdecomp.cpp
        unkdecomp.h)
