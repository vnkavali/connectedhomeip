#!/usr/bin/env bash

set -eu
readonly ROOT_PATH=$(cd $(dirname $0) && pwd)
#readonly LOCAL_ENV_PATH=${ROOT_PATH}/third_party
readonly LOCAL_ENV_PATH=/home/chayan/python_webrtc_client/libwebrtc_125
mkdir -p build
oldpath=`pwd`
cd build

cmake -DLIBWEBRTC_PATH=${LOCAL_ENV_PATH} ..
make

g++ -O3 -Wall -shared -std=gnu++17 -undefined -fPIC -I/usr/include/python3.10 -I/home/chayan/.local/lib/python3.10/site-packages/pybind11/include -o pybind.so -I ../include ../src/pybind.cpp -L../build -lwebrtc

cd $oldpath
