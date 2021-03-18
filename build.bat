mkdir build\
set PREFIX=C:/
set CCFLAGS=-IC:/include
set LDFLAGS=-LC:/lib
meson setup build --buildtype=release --prefix C:/
cd build
ninja
cd ..