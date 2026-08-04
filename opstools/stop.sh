#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"

# ===== 程序配置 =====
APP_NAME="dce_combo_radar"

PID_FILE="$DIR/${APP_NAME}.pid"


[ -f "$PID_FILE" ] || exit 0


PID=$(cat "$PID_FILE")


if ! kill -0 "$PID" 2>/dev/null; then
    rm -f "$PID_FILE"
    exit 0
fi


echo "Sending SIGTERM to $APP_NAME PID $PID..."

kill -TERM "$PID" 2>/dev/null || true


for i in $(seq 1 20); do
    if ! kill -0 "$PID" 2>/dev/null; then
        echo "$APP_NAME terminated gracefully."
        rm -f "$PID_FILE"
        exit 0
    fi

    sleep 0.5
done


echo "Sending SIGKILL to $APP_NAME PID $PID..."

kill -9 "$PID" 2>/dev/null || true


sleep 1

rm -f "$PID_FILE"

echo "$APP_NAME stopped."