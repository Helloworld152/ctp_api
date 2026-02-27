#!/bin/bash
cd build
make
cd ..
mkdir -p flow_md
LD_LIBRARY_PATH="$(pwd)/lib:$LD_LIBRARY_PATH" ./build/bin/md_client "$@"