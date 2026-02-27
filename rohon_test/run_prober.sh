#!/bin/bash
cd $(dirname $0)
export LD_LIBRARY_PATH=./lib:$LD_LIBRARY_PATH
mkdir -p flow_probe
echo "开始探测..."
./auth_prober
