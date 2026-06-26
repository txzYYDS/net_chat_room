#!/bin/bash
# ============================================================
#  net_chat 一键停止
#  1. 断开 serveo.net 隧道
#  2. 停止 bridge.js 中转站
#  3. 停止 C 服务端
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

PID_FILE="$SCRIPT_DIR/.run.pid"

echo "======================================================"
echo "  net_chat 停服"
echo "======================================================"

if [ ! -f "$PID_FILE" ]; then
    echo "[!] 没有找到 .run.pid 文件，可能没有正在运行的服务"
    echo "    尝试扫描残留进程..."
    echo ""

    # 兜底：扫描进程
    FOUND=0

    # SSH 隧道
    SSH_PID=$(pgrep -f "ssh.*serveo\.net" 2>/dev/null | head -1)
    if [ -n "$SSH_PID" ]; then
        echo "      停止 serveo.net 隧道 (PID: $SSH_PID)"
        kill "$SSH_PID" 2>/dev/null
        FOUND=1
    fi

    # bridge.js
    BRIDGE_PID=$(pgrep -f "node bridge.js" 2>/dev/null | head -1)
    if [ -n "$BRIDGE_PID" ]; then
        echo "      停止 bridge.js (PID: $BRIDGE_PID)"
        kill "$BRIDGE_PID" 2>/dev/null
        FOUND=1
    fi

    # C 服务端
    SERVER_PID=$(pgrep -f "net_chat_server" 2>/dev/null | head -1)
    if [ -n "$SERVER_PID" ]; then
        echo "      停止 C 服务端 (PID: $SERVER_PID)"
        kill "$SERVER_PID" 2>/dev/null
        FOUND=1
    fi

    if [ "$FOUND" -eq 0 ]; then
        echo "      没有发现残留进程"
    fi

    echo ""
    echo "======================================================"
    echo "  清理完成"
    echo "======================================================"
    exit 0
fi

# 读取 PID 文件（第 4 行是项目路径）
readarray -t LINES < "$PID_FILE"
SERVER_PID="${LINES[0]}"
BRIDGE_PID="${LINES[1]}"
TUNNEL_PID="${LINES[2]}"

# --- 1. 断开 serveo.net 隧道 ---
echo "[1/3] 断开 serveo.net 隧道..."
if [ -n "$TUNNEL_PID" ] && kill -0 "$TUNNEL_PID" 2>/dev/null; then
    kill "$TUNNEL_PID" 2>/dev/null
    echo "      SSH 隧道已断开 (PID: $TUNNEL_PID)"
else
    echo "      SSH 隧道未在运行"
    # 兜底：按进程名杀
    SSH_PID=$(pgrep -f "ssh.*serveo\.net" 2>/dev/null | head -1)
    if [ -n "$SSH_PID" ]; then
        kill "$SSH_PID" 2>/dev/null
        echo "      已通过进程名断开"
    fi
fi

# --- 2. 停止 bridge.js ---
echo "[2/3] 停止 bridge.js..."
if [ -n "$BRIDGE_PID" ] && kill -0 "$BRIDGE_PID" 2>/dev/null; then
    kill "$BRIDGE_PID" 2>/dev/null
    echo "      bridge.js 已停止 (PID: $BRIDGE_PID)"
else
    echo "      bridge.js 未在运行"
    BRIDGE_PID=$(pgrep -f "node bridge.js" 2>/dev/null | head -1)
    if [ -n "$BRIDGE_PID" ]; then
        kill "$BRIDGE_PID" 2>/dev/null
        echo "      已通过进程名停止"
    fi
fi

# --- 3. 停止 C 服务端 ---
echo "[3/3] 停止 C 服务端..."
if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
    kill "$SERVER_PID" 2>/dev/null
    echo "      C 服务端已停止 (PID: $SERVER_PID)"
else
    echo "      C 服务端未在运行"
    SERVER_PID=$(pgrep -f "net_chat_server" 2>/dev/null | head -1)
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null
        echo "      已通过进程名停止"
    fi
fi

# 清理 PID 文件
rm -f "$PID_FILE"

echo ""
# 确认端口已释放
if netstat -tlnp 2>/dev/null | grep -qE ":(8888|3000).*LISTEN"; then
    echo "[!] 警告：部分端口未释放，可能需要手动处理"
else
    echo "[OK] 端口 8888 / 3000 已释放"
fi

echo "======================================================"
echo "  所有服务已停止"
echo "======================================================"
