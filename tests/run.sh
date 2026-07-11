#!/bin/bash

# activate python virtual env if not already active
source ../.venv/bin/activate

source ../bmpto105/compile.sh $1

set -e

g++ -o bmpto105_test bmpto105_test.cpp
echo "✅ bmpto105_test.cpp compilation successful!"

# the actual test
./bmpto105_test ./castle.png
