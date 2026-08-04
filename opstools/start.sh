#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR" || exit 1 

# ===== 程序配置 =====
APP_NAME="dce_combo_radar"
APP="$DIR/$APP_NAME"

PID_FILE="$DIR/${APP_NAME}.pid"
LOG_DIR="$DIR/logs"

ARGS=(
    "$DIR/xs2_mirror.aeg"
    "$DIR/symbols.txt"
)

LOG_TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/${APP_NAME}_${LOG_TIMESTAMP}.log"

mkdir -p "$LOG_DIR"


# ===== 启动前检查 =====
if [ -f "$PID_FILE" ]; then
    echo "Old PID file found. Cleaning up previous instance..."
    bash "$DIR/stop.sh"
    sleep 1
fi


echo "Starting $APP_NAME..."
echo "Log file: $LOG_FILE"


nohup "$APP" "${ARGS[@]}" >> "$LOG_FILE" 2>&1 &

PID=$!


sleep 0.5


if kill -0 "$PID" 2>/dev/null; then
    echo "$PID" > "$PID_FILE"
    echo "$APP_NAME started successfully (PID: $PID)"
else
    echo "ERROR: $APP_NAME failed to start!" >&2
    exit 1
fi


# 清理30天日志
find "$LOG_DIR" \
    -maxdepth 1 \
    -name "*.log" \
    -mtime +30 \
    -delete
