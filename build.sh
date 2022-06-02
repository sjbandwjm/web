#!/bin/bash

BUILD_TYPE=$1
if [[ "${BUILD_TYPE}" == "" ]]; then
    BUILD_TYPE=Release
fi

cd thirdparty/brpc
git apply --check ../../patch/find_package_patch.patch && git apply ../../patch/find_package_patch.patch
cd ../..

cp patch/usockets_patch.patch thirdparty/uWebsockets/uSockets/CMakeLists.txt

mkdir -p build && cd build
conan install .. --output-folder=. --build=missing -nr
cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE} && make -j ${nproc}
