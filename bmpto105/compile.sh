#!/bin/bash
# execute in script's directory
OLD_CD=$PWD
cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null || exit 1

# activate python virtual env if not already active
if [ -f "../.venv/bin/activate" ]; then
	echo "Virtualenv detected, activating it..."
	source ../.venv/bin/activate
fi

if [ "$1" = '--force' ]; then
	echo "Removing old library to recompile..."
	rm libbmpto105.so 2> /dev/null || true
fi

if [ ! -f "libbmpto105.so" ]; then
	echo "Compiling bmpto105 module..."
	set -e
	g++ -O3 -Wall -shared -std=c++20 -fPIC -Wbuiltin-macro-redefined -Wunused-function \
	    $(python3 -m pybind11 --includes) $(python3-config --includes --ldflags) \
	    libbmpto105.cpp bmpto105_py.cpp -o libbmpto105.so # -D_USE_CONSOLE_ -D_USE_DEBUG_
	echo "✅ libbmpto105 compilation successful!"
fi

cd -- $OLD_CD
