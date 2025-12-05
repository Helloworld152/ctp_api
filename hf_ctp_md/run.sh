#!/bin/bash

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_DIR="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_DIR/build"
BIN_NAME="hf_ctp_md"
BIN_PATH="$BUILD_DIR/$BIN_NAME"
LIB_DIR="$PROJECT_DIR/lib"
FLOW_DIR="$PROJECT_DIR/flow"

# 1. 确保 flow 目录存在 (CTP API 产生流文件需要)
if [ ! -d "$FLOW_DIR" ]; then
    echo "[Script] Creating flow directory..."
    mkdir -p "$FLOW_DIR"
fi

# 2. 检查并构建项目
if [ ! -f "$BIN_PATH" ]; then
    echo "[Script] Executable not found. Building project..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. || exit 1
    make -j$(nproc) || exit 1
    cd "$PROJECT_DIR"
    echo "[Script] Build complete."
else
    # 如果存在但想重新编译，可以加参数，这里简单处理：
    # 如果 build 目录存在但没 make，这里不做增量检查，由用户手动 make
    echo "[Script] Executable found."
fi

# 3. 设置环境变量
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIB_DIR

# 4. 运行程序
echo "[Script] Starting $BIN_NAME..."
echo "----------------------------------------"
# 切换到项目根目录运行，确保相对路径 (如 ./flow/) 正确
cd "$PROJECT_DIR"
exec "$BIN_PATH" "$@"
