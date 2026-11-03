#!/bin/bash
set -e
cd "$(dirname "$0")"

if command -v cmake >/dev/null 2>&1; then
    mkdir -p build
    cd build
    cmake .. >/dev/null
    make
    echo "编译完成：$(pwd)/rectangle"
else
    g++ -O2 -std=c++17 main.cpp detector.cpp output.cpp -o rectangle `pkg-config --cflags --libs opencv4`
    echo "编译完成：$(pwd)/rectangle"
fi
