#!/bin/bash

VER=`git rev-list HEAD | wc -l`
echo "#define VERSION_GIT $VER" > version.h
gcc avs4x264mod.c -s -O3 -std=gnu99 -ffast-math -oavs4x264mod -Wl,--large-address-aware
rm -f version.h
