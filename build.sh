#!/bin/bash
set -e

ml qmio/hpc gcc/12.3.0 qmio-qiskitcpp/896f9fa-python-3.11.9

cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build
