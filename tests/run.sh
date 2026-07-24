#!/bin/bash

# activate python virtual env if not already active
if [[ ! -n "$VIRTUAL_ENV" ]]; then
	if [ -f "../.venv/bin/activate" ]; then
		echo "Virtualenv detected but not active, activating it..."
		source ../.venv/bin/activate
	fi  
fi

source ../bmpto105/compile.sh $1

set -e

g++ -O0 --debug -o test -Wall -std=c++20 test.cpp -I../bmpto105 -L../bmpto105 -lbmpto105 \
	$(python3 -m pybind11 --includes) $(python3-config --includes --ldflags) -lpython3.14

echo "✅ test.cpp compilation successful!"

# the actual test
LD_LIBRARY_PATH=../bmpto105/ ./test ./castle.png
