#!/bin/bash
# ============================================================
#  net_chat 一键启动
#  1. 编译并启动 C 服务端 (port 8888)
#  2. 启动 bridge.js 中转站 (port 3000)
#  3. 通过 serveo.net 获取免费域名
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

PID_FILE="$SCRIPT_DIR/.run.pid"
LOG_DIR="$SCRIPT_DIR/logs"

mkdir -p "$LOG_DIR"

# ── 启动前清理残留进程和过期 PID 文件 ──────────────────────────
cleanup_stale() {
    local STALE=0
    # 杀掉残留 C 服务端（占用 8888）
    OLD_SERVER=$(pgrep -f "net_chat_server" 2>/dev/null | head -1)
    if [ -n "$OLD_SERVER" ]; then
        echo "  [清理] 发现残留 C 服务端 (PID: $OLD_SERVER)，已终止"
        kill "$OLD_SERVER" 2>/dev/null; STALE=1
    fi
    # 杀掉残留 bridge.js（占用 3000）
    OLD_BRIDGE=$(pgrep -f "node bridge.js" 2>/dev/null | head -1)
    if [ -n "$OLD_BRIDGE" ]; then
        echo "  [清理] 发现残留 bridge.js (PID: $OLD_BRIDGE)，已终止"
        kill "$OLD_BRIDGE" 2>/dev/null; STALE=1
    fi
    # 杀掉残留 SSH 隧道
    OLD_SSH=$(pgrep -f "ssh.*serveo\.net" 2>/dev/null | head -1)
    if [ -n "$OLD_SSH" ]; then
        echo "  [清理] 发现残留 SSH 隧道 (PID: $OLD_SSH)，已终止"
        kill "$OLD_SSH" 2>/dev/null; STALE=1
    fi
    # 清理过期 PID 文件
    if [ -f "$PID_FILE" ]; then
        rm -f "$PID_FILE"
        STALE=1
    fi
    [ "$STALE" -eq 1 ] && sleep 1   # 给系统时间释放端口
}
cleanup_stale

# --- 清理函数，脚本异常退出时也尝试停掉已启动的服务 ---
cleanup_on_error() {
    echo ""
    echo "[!] 启动过程出错，尝试清理已启动的服务..."
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2>/dev/null
    [ -n "$BRIDGE_PID" ] && kill "$BRIDGE_PID" 2>/dev/null
    [ -n "$TUNNEL_PID" ] && kill "$TUNNEL_PID" 2>/dev/null
    rm -f "$PID_FILE"
}
trap cleanup_on_error ERR INT

# ============================================
# 1. C 服务端
# ============================================
echo "======================================================"
echo "  net_chat 一键启动"
echo "======================================================"

if [ ! -f "build/output/net_chat_server" ]; then
    echo "[1/3] 编译 C 服务端..."
    make clean && make
else
    echo "[1/3] C 服务端已编译，跳过 make"
fi

echo "      启动 C 服务端 (port 8888)..."
./build/output/net_chat_server > "$LOG_DIR/server.log" 2>&1 &
SERVER_PID=$!

# 等待端口就绪（最多 5 秒）
for i in $(seq 1 10); do
    if netstat -tlnp 2>/dev/null | grep -q ":8888.*LISTEN"; then
        echo "      C 服务端已启动 (PID: $SERVER_PID)"
        break
    fi
    sleep 0.5
done

# ============================================
# 2. bridge.js 中转站
# ============================================
echo "[2/3] 启动 bridge.js 中转站 (port 3000)..."
node bridge.js > "$LOG_DIR/bridge.log" 2>&1 &
BRIDGE_PID=$!

# 等待端口就绪（最多 5 秒）
for i in $(seq 1 10); do
    if netstat -tlnp 2>/dev/null | grep -q ":3000.*LISTEN"; then
        echo "      bridge.js 已启动 (PID: $BRIDGE_PID)"
        break
    fi
    sleep 0.5
done

# ============================================
# 3. serveo.net 免费域名隧道
# ============================================
echo "[3/3] 获取免费域名 (serveo.net)..."

TUNNEL_LOG="$LOG_DIR/tunnel.log"

# 关键：不能加 -N（会阻止 serveo 创建会话 → 不发欢迎消息）
# -tt 强制 PTY → serveo 的 "Forwarding HTTP traffic from..." 消息才会出现
# 2>&1 | tee → 行缓冲，实时写文件 + 终端显示
ssh -o StrictHostKeyChecking=no -tt -R 80:localhost:3000 serveo.net 2>&1 | tee "$TUNNEL_LOG" &
TUNNEL_PID=$!

# 等待域名出现（最多 20 秒）
DOMAIN=""
for i in $(seq 1 40); do
    DOMAIN=$(grep -oP 'https://[^\s]+' "$TUNNEL_LOG" 2>/dev/null | head -1)
    if [ -n "$DOMAIN" ]; then
        break
    fi
    sleep 0.5
done

echo "  隧道已启动 (PID: $TUNNEL_PID)"

# 记录 PID
echo "$SERVER_PID"  >  "$PID_FILE"
echo "$BRIDGE_PID"  >> "$PID_FILE"
echo "$TUNNEL_PID"  >> "$PID_FILE"
echo "$SCRIPT_DIR"  >> "$PID_FILE"

echo ""
echo "======================================================"
echo "  启动完成！"
echo "  服务 PID:"
echo "    C 服务端:    $SERVER_PID  (日志: logs/server.log)"
echo "    bridge.js:  $BRIDGE_PID  (日志: logs/bridge.log)"
echo "    SSH 隧道:    $TUNNEL_PID"
echo ""
if [ -n "$DOMAIN" ]; then
    echo "  🌐 免费域名: $DOMAIN"
else
    echo "  🌐 域名获取中，请查看: cat logs/tunnel.log"
fi
echo ""
echo "  停止服务: ./stop.sh"
echo "======================================================"

# 保持前台运行，按 Ctrl+C 可手动停止
wait $TUNNEL_PID
