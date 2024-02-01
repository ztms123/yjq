#!/bin/bash
$(cd "$(dirname $0)";pwd)
rm my_web.exe
rm -rf build
mkdir build
cd build
cmake ..
make
cp my_web.exe ../