#!/bin/bash

# activate python virtual env if not already active
source ../.venv/bin/activate

source ../bmpto105/compile.sh $1

set -e

g++ -O3 -o bmpto105_test -Wall -std=c++20 bmpto105_test.cpp -I../bmpto105 -L../bmpto105 -lbmpto105 \
	$(python3 -m pybind11 --includes) $(python3-config --includes --ldflags) -lpython3.14 -D_USE_CONSOLE_ -D_USE_DEBUG_

echo "✅ bmpto105_test.cpp compilation successful!"

# the actual test
LD_LIBRARY_PATH=../bmpto105/ ./bmpto105_test ./castle.png
