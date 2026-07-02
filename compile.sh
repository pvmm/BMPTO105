#!/bin/bash
# compile.sh

# Stop on error
set -e

echo "Compiling bmpto105 module..."
g++ -O3 -Wall -shared -std=c++17 -fPIC -Wbuiltin-macro-redefined -Wunused-function \
    $(python3 -m pybind11 --includes) $(python3-config --includes --ldflags) \
    bmpto105_lib.cpp bmpto105_py.cpp -o bmpto105.so


echo "✅ Compilation successful!"
echo "You can now import bmpto105 in Python."
python bmpto105.py
