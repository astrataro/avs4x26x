#!/bin/bash

VER=`git rev-list HEAD | wc -l`
echo "#define VERSION_GIT $VER" > version.h
gcc avs4x264mod.c -s -O3 -ffast-math -oavs4x264mod-laa -Wl,--large-address-aware
gcc avs4x264mod.c -s -O3 -ffast-math -oavs4x264mod
rm -f version.h
