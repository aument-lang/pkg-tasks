#!/bin/bash
mkdir -p build
meson setup build --buildtype=release
cd build
ninja
cd ..