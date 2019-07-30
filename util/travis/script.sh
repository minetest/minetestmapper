#!/bin/bash -e
mkdir -p travisbuild
cd travisbuild

cmake .. \
	-DENABLE_LEVELDB=1

make -j2
