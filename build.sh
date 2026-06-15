#!/bin/bash
set -e

ml qmio/hpc gcccore/12.3.0 python/3.11.9

cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build