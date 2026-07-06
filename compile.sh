#!/bin/bash

# activate python virtual env if not already active
source .venv/bin/activate

# Stop on error
set -e

if [ ! -f "bmpto105/libbmpto105.so" ]; then
	echo "Compiling bmpto105 module..."
	g++ -O3 -Wall -shared -std=c++20 -fPIC -Wbuiltin-macro-redefined -Wunused-function \
	    $(python3 -m pybind11 --includes) $(python3-config --includes --ldflags) \
	    bmpto105/libbmpto105.cpp bmpto105/bmpto105_py.cpp -o bmpto105/libbmpto105.so
	echo "✅ Compilation successful!"
fi

echo "You can now import bmpto105 in Python."
python tests/bmpto105_test.py castle.png
