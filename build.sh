#!/bin/bash

BUILD_TYPE=$1
if [[ "${BUILD_TYPE}" == "" ]]; then
    BUILD_TYPE=Debug
fi

mkdir -p build && cd build && cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -S .. -B . && make -j $(nproc)