# net_chat — C 语言聊天室（Web 版）

基于 C 语言 TCP + select 的聊天室，通过 Node.js 桥接层实现网页端访问。

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     浏览器（任意设备）                        │
│              HTML + CSS + JavaScript                        │
│    • 暗色玻璃拟态 UI                                       │
│    • WebSocket 通信                                        │
└──────────────────────┬────────────────────────────────────┘
                       │  WebSocket (ws://host:3000)
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                bridge.js（Node.js 桥接层）                  │
│                                                             │
│  • HTTP 静态文件服务（public/index.html）                   │
│  • WebSocket 握手（RFC 6455 手写实现）                    │
│  • WebSocket 帧解析 / 组装                                │
│  • TCP 转发：浏览器消息 → C 服务端                        │
│  • TCP 转发：C 服务端消息 → 浏览器                        │
└──────────────────────┬────────────────────────────────────┘
                       │  TCP / select (port 8888)
                       ▼
┌─────────────────────────────────────────────────────────────┐
│                  C 服务端（net_chat_server）                 │
│                                                             │
│  • socket() + bind() + listen()                           │
│  • select() 多路复用                                      │
│  • 双向链表管理客户端（list.h / client_info.c）             │
│  • 消息广播（遍历链表 send()）                             │
│  • 支持 TCP 直连 和 通过 bridge 连接的 Web 客户端         │
└─────────────────────────────────────────────────────────────┘
```

---

## 三种客户端接入方式

| 方式 | 连接路径 | 说明 |
|------|---------|------|
| **TCP 直连** | `telnet 192.168.3.56 8888` | 传统终端客户端，输入名字后收发消息 |
| **Web 浏览器** | `http://192.168.3.56:3000` | 通过 bridge.js 桥接，同一 C 服务端 |
| **C 客户端（待实现）** | TCP → 8888 | 可编写带 ncurses 的终端 UI 客户端 |

> **注意**：TCP 直连客户端和 Web 客户端可以同时在线，互相收发消息。

---

## 文件结构

```
net_chat/
├── Makefile                  # 编译配置
├── README.md                # 项目说明（本文件）
├── bridge.js               # Node.js 桥接层（WebSocket ↔ TCP）
├── net_chat_server.c       # C 服务端主程序
├── .gitignore
├── inc/                    # 头文件
│   ├── client_info.h       # 客户端结构体、链表操作声明
│   ├── common.h           # 公共宏定义
│   ├── list.h             # 双向链表实现（内核式 list API）
│   └── net_connect.h      # socket / select 头文件
├── src/                    # C 源文件
│   ├── client_info.c      # 客户端管理、链表操作实现
│   └── list.c             # 链表基础操作实现
└── public/
    └── index.html          # 网页聊天室前端（单文件）
```

---

## 编译与运行

### 1. 编译 C 服务端

```bash
cd ~/txz_code_test/work/DIY/net_chat
make clean && make
```

编译产物：`build/output/net_chat`

### 2. 启动 C 服务端

```bash
./build/output/net_chat_server
```

成功输出：
```
原神...启动!!!!!!!!!!
```

### 3. 启动桥接层（Web 支持）

**新开一个终端：**

```bash
cd ~/txz_code_test/work/DIY/net_chat
node bridge.js
```

成功输出：
```
======================================================
  net_chat 聊天室 — Web 桥接层 v8
======================================================
  浏览器打开: http://localhost:3000
======================================================
```

### 4. 防火墙放行（局域网访问需要）

```bash
sudo ufw allow 3000/tcp   # Web 端口
sudo ufw allow 8888/tcp   # C 服务端端口（TCP 直连用）
```

### 5. 打开浏览器

- 本机访问：`http://localhost:3000`
- 局域网访问：`http://192.168.3.56:3000`（替换为你的 Linux IP）

---

## 协议说明

### 浏览器 ↔ bridge.js（WebSocket JSON）

| type | 方向 | 说明 |
|------|------|------|
| `join` | 浏览器 → bridge | `{ type:"join", name:"张三", psk:"123456", register:false }` 登录/注册 |
| `joined` | bridge → 浏览器 | `{ type:"joined", text:"欢迎..." }` 加入成功 |
| `msg` | 浏览器 → bridge | `{ type:"msg", text:"你好" }` 发送消息 |
| `msg` | bridge → 浏览器 | `{ type:"msg", text:"[张三]: 你好" }` 收到消息 |
| `system` | bridge → 浏览器 | `{ type:"system", text:"..." }` 系统通知 |
| `error` | bridge → 浏览器 | `{ type:"error", text:"..." }` 错误信息 |

### bridge.js ↔ C 服务端（TCP 明文）

```
# 加入时，bridge 先发用户名，再发密码（各自以 \n 结尾）
bridge → C:  "张三\n"
bridge → C:  "123456\n"

# 聊天时，bridge 转发消息（以 \r\n 结尾）
bridge → C:  "[张三]: 你好\r\n"

# C 服务端广播消息（以 \r\n 结尾）
C → bridge:  "用户 张三 加入了聊天室\r\n"
C → bridge:  "[张三]: 你好\r\n"
```

---

## C 服务端核心逻辑

```
main()
  ├── socket()          创建监听套接字
  ├── bind(port=8888)  绑定端口
  ├── listen()          开始监听
  └── while(1)
        └── select()    多路复用，监听所有 fd
              ├── 新连接 → accept() → 加入链表 → 发送欢迎
              ├── 客户端消息处理（三级状态机）：
              │     ① name 为空 → 收用户名
              │     ② psk  为空 → 收密码 → 登录成功 → 广播加入
              │     ③ 都已设置 → 聊天消息 → 遍历链表广播
              └── 客户端断开 → 从链表移除 → 广播离开
```

**链表结构：**

```c
struct manager_device {        // 客户端管理器
    struct list_entry *head;  // 链表头（环形双向链表）
};

struct client_info {           // 单个客户端
    int fd;                  // 套接字 fd
    char name[50];           // 用户名
    char psk[32];            // 用户密码
    struct list_entry entry;  // 链表节点
};
```

---

## 依赖

| 组件 | 依赖 | 说明 |
|------|------|------|
| C 服务端 | Linux / POSIX | socket、select、pthread（可选） |
| bridge.js | Node.js 12+ | 仅用原生模块，零 npm 依赖 |
| 前端 | 现代浏览器 | 支持 WebSocket 和 ES6 |

---

## 已知问题 / TODO

- [x] ~~用户密码登录~~（2025-06-25 已完成：psk 字段 + 三级状态机登录流程）
- [ ] `net_chat_client.c` 尚未实现（可编写 ncurses 终端 UI 客户端）
- [ ] bridge.js 的 WebSocket 帧解析不支持超过 64KB 的大帧
- [ ] C 服务端的 `manager->head` 内存分配后未释放（程序退出时系统回收）
- [ ] 用户名重复检测未实现
- [ ] 消息历史记录未实现（新用户看不到之前的消息）

---

## 作者

无 — 基于 C 语言 TCP + select 实现，Web 桥接层用 Node.js 手写 WebSocket 协议。
