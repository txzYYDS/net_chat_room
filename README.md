# net_chat — C 语言聊天室（Web 版）

基于 C 语言 TCP + select 的聊天室，通过 Node.js 桥接层实现网页端访问，用户数据持久化于 JSON 文件。

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                     浏览器（任意设备）                        │
│              HTML + CSS + JavaScript                        │
│    • 马卡龙色系玻璃拟态 UI + 粉色渐变气泡                 │
│    • 表情选择器（400+ Unicode 表情）                        │
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
│  • 用户认证：注册/登录（PBKDF2 + SHA-512 密码哈希）       │
│  • JSON 文件用户数据持久化（users.json，零依赖）             │
│  • TCP 转发：浏览器消息 → C 服务端                        │
│  • TCP 转发：C 服务端消息 → 浏览器                        │
└──────────────┬──────────────────┬──────────────────────────┘
               │  TCP (port 8888) │  FS (users.json)
               ▼                  ▼
┌──────────────────────────────┐  ┌──────────────────────────┐
│  C 服务端（net_chat_server） │  │  users.json              │
│                              │  │                          │
│  • socket + bind + listen   │  │  { "张三": {             │
│  • select() 多路复用        │  │      "hash": "abc123...",│
│  • 双向链表管理客户端       │  │      "salt": "def456...",│
│  • 消息广播（遍历链表）     │  │      "created":"..."     │
│  • 二级状态机（用户名→聊天） │  │    },                    │
│  • TCP 直连 + Web 客户端    │  │    ...                   │
└──────────────────────────────┘  │  }                       │
                                  └──────────────────────────┘
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

## 前端功能

| 功能 | 说明 |
|------|------|
| **用户系统** | 登录/注册面板，密码输入支持显示/隐藏切换 |
| **马卡龙粉紫主题** | 清新玻璃拟态 + 两面气泡统一粉色渐变 + 月薪喵装饰 |
| **表情选择器** | 8 个分类、400+ Unicode 表情，点击插入光标位置，Esc/点击外部关闭 |
| **纯表情放大** | 消息仅为表情时，自动去掉气泡背景，表情放大至 42px 展示 |
| **响应式布局** | 移动端自动适配：全屏聊天、7 列表情网格、缩小字号 |

---

## 文件结构

```
net_chat/
├── Makefile                  # 编译配置
├── README.md                # 项目说明（本文件）
├── bridge.js               # Node.js 桥接层（WebSocket ↔ TCP + JSON 用户存储）
├── users.json              # JSON 用户数据库（自动创建，已 gitignore）
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
    └── index.html          # 网页聊天室前端（单文件，含 HTML/CSS/JS）
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

**（首次无需额外安装，零 npm 依赖）**

```bash
cd ~/txz_code_test/work/DIY/net_chat
node bridge.js
```

成功输出：
```
======================================================
  net_chat 聊天室 — Web 桥接层 v10
======================================================
  浏览器打开: http://localhost:3000
======================================================
[db] ✓ 已加载用户数据 (0 位用户)
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

## 用户认证流程

```
┌──────────┐    注册/登录     ┌───────────┐    查询/写入    ┌──────────┐
│  浏览器   │ ─────────────→ │ bridge.js │ ─────────────→ │ users.db │
│          │                 │           │                │ (SQLite) │
│  {name,   │                 │ 检查重复  │                │          │
│   psk,    │                 │ 验证密码  │                │ PBKDF2   │
│   register│                 │ 建立TCP   │                │ 哈希存储  │
│  }        │ ←───────────── │           │ ←───────────── │          │
└──────────┘   结果（成功/    └───────────┘   查询结果      └──────────┘
               失败原因）
                       │
                       │ 认证通过
                       ▼
              ┌─────────────────┐
              │  C 服务端 (TCP)  │
              │  发送 name      │
              │  进入聊天室      │
              └─────────────────┘
```

### 注册

1. 浏览器发送 `{ type:"join", name:"张三", psk:"mypass", register:true }`
2. bridge 查 `users.db`：用户名是否已存在
3. 不存在 → PBKDF2 哈希密码 → `INSERT` 入库 → 连接 C 服务端
4. 已存在 → 返回错误 "用户名已被占用"

### 登录

1. 浏览器发送 `{ type:"join", name:"张三", psk:"mypass", register:false }`
2. bridge 查 `users.db`：`SELECT hash, salt FROM users WHERE name = ?`
3. 用户不存在 → 返回错误 "用户不存在，请先注册"
4. 用户存在 → PBKDF2 验证密码 → 匹配成功则连接 C 服务端
5. 密码不匹配 → 返回错误 "密码错误"

### 密码安全

- **算法**：PBKDF2 + SHA-512
- **迭代次数**：10,000 轮
- **盐**：每次注册生成 16 字节随机盐
- **存储**：`users.db` 中只存哈希和盐值，**绝不存储明文密码**

---

## 协议说明

### 浏览器 ↔ bridge.js（WebSocket JSON）

| type | 方向 | 说明 |
|------|------|------|
| `join` | 浏览器 → bridge | `{ type:"join", name:"张三", psk:"123456", register:false }` 登录/注册 |
| `joined` | bridge → 浏览器 | `{ type:"joined", text:"欢迎进入聊天室！" }` 加入成功 |
| `msg` | 浏览器 → bridge | `{ type:"msg", text:"你好😊" }` 发送消息（支持 Unicode 表情） |
| `msg` | bridge → 浏览器 | `{ type:"msg", text:"[张三]: 你好😊" }` 收到消息 |
| `system` | bridge → 浏览器 | `{ type:"system", text:"..." }` 系统通知 |
| `error` | bridge → 浏览器 | `{ type:"error", text:"..." }` 错误信息 |

### bridge.js ↔ C 服务端（TCP 明文）

```
# 加入时，bridge 发送用户名（以 \n 结尾）
bridge → C:  "张三\n"

# 聊天时，bridge 转发消息（以 \r\n 结尾）
bridge → C:  "[张三]: 你好😊\r\n"

# C 服务端广播消息（以 \r\n 结尾）
C → bridge:  "用户 张三 加入了聊天室\r\n"
C → bridge:  "[张三]: 你好😊\r\n"
```

> **注意**：表情符号（Emoji）作为普通 Unicode 文本传输，无需任何特殊处理。bridge.js 和 C 服务端均无需修改即可支持表情。

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
              ├── 客户端消息处理（二级状态机）：
              │     ① name 为空 → 收用户名 → 广播加入
              │     ② name 已设置 → 聊天消息 → 遍历链表广播
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
    struct list_entry entry;  // 链表节点
};
```

---

## 数据库

### 存储格式（users.json）

```json
{
  "张三": {
    "hash":    "a1b2c3d4...",
    "salt":    "e5f6g7h8...",
    "created": "2025-06-25 19:30:00"
  },
  "李四": {
    "hash":    "i9j0k1l2...",
    "salt":    "m3n4o5p6...",
    "created": "2025-06-25 20:15:00"
  }
}
```

> 以用户名为 key，查找 O(1)，写入直接 `JSON.stringify` 落盘。

### 技术细节

| 项 | 选择 |
|------|------|
| 引擎 | JSON 文件（Node.js 原生 `fs` 模块，零 npm 依赖） |
| 文件 | `users.json`（项目根目录，自动创建，已加入 .gitignore） |
| 密码存储 | PBKDF2 + SHA-512 + 16 字节随机盐 + 10000 轮迭代 |

---

## 依赖

| 组件 | 依赖 | 说明 |
|------|------|------|
| C 服务端 | Linux / POSIX | socket、select、pthread（可选） |
| bridge.js | Node.js 12+ | 仅用原生模块（http、fs、net、crypto），零 npm 依赖 |
| 前端 | 现代浏览器 | 支持 WebSocket 和 ES6 |

---

## 已知问题 / TODO

- [x] 用户密码登录（bridge.js 端 PBKDF2 验证，C 服务端 psk 字段已移除）
- [x] 用户数据持久化（2025-06-25 完成：SQLite + PBKDF2 密码哈希）
- [x] 表情包发送（2025-06-25 完成：Unicode 表情选择器，8 分类 400+ 表情）
- [x] 马卡龙色系主题统一（粉紫渐变 + 月薪喵装饰 + 小清新背景）
- [ ] `net_chat_client.c` 尚未实现（可编写 ncurses 终端 UI 客户端）
- [ ] bridge.js 的 WebSocket 帧解析不支持超过 64KB 的大帧
- [ ] C 服务端的 `manager->head` 内存分配后未释放（程序退出时系统回收）
- [ ] 消息历史记录未实现（新用户看不到之前的消息）
- [ ] 用户在线状态 / 用户列表未实现
- [ ] 私聊功能未实现

---

## 版本历史

| 日期 | 版本 | 变更 |
|------|------|------|
| 2025-06-25 | v10 | SQLite 改为 JSON 文件存储（users.json），彻底零 npm 依赖 |
| 2025-06-25 | v9 | 新增 SQLite 用户数据库、PBKDF2 密码哈希、注册/登录验证 |
| 2025-06-25 | v8 | 新增表情选择器（8 分类 400+ 表情）、紫色渐变主题统一 |
| 2025-06-25 | v7 | 新增用户密码（psk）字段、三级状态机登录流程（已合并为二级状态机） |
| 2025-06-25 | v6 | 首版 Web 聊天室（方案 A：C + Node.js 桥接 + Web 前端） |

---

## 作者

无 — 基于 C 语言 TCP + select 实现，Web 桥接层用 Node.js 手写 WebSocket 协议 + JSON 文件用户存储。
