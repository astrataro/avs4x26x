#!/bin/bash

VER=`git rev-list HEAD | wc -l`
echo "#define VERSION_GIT $VER" > version.h
gcc avs4x26x.c -s -O3 -std=gnu99 -ffast-math -oavs4x26x -Wl,--large-address-aware
x86_64-w64-mingw32-gcc avs4x26x.c -s --3 -std=gnu99 -ffast-math -oavs4x26x-x64
rm -f version.h
