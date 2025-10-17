#!/bin/bash

# 设置库路径并运行
LD_LIBRARY_PATH="$(pwd)/lib:$LD_LIBRARY_PATH" ./build/bin/md_client