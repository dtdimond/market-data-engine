#!/bin/bash
set -e

# Clean and build
rm -rf build
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

echo ""
echo "Build complete! Run with: ./build/market_data_engine"
