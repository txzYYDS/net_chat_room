/*
 * net_chat Web 桥接层 v10
 *
 * 浏览器 ←WebSocket→ bridge.js ←TCP→ C 服务端
 * 用户数据 → users.json（JSON 文件，零依赖）
 *
 * 用法: node bridge.js
 * 浏览器: http://localhost:3000
 */

'use strict';

var http   = require('http');
var fs     = require('fs');
var path   = require('path');
var net    = require('net');
var crypto = require('crypto');

// ==================== 配置 ====================
var WEB_PORT  = 3000;
var CHAT_HOST = '127.0.0.1';
var CHAT_PORT = 8888;
var DB_PATH   = path.join(__dirname, 'users.json');

// ==================== JSON 文件用户存储（零依赖）====================
// users.json 格式: { "用户名": { "hash":"...", "salt":"...", "created":"..." }, ... }
function loadUsers() {
  try {
    var raw = fs.readFileSync(DB_PATH, 'utf8');
    return JSON.parse(raw);
  } catch (e) {
    return {};
  }
}

function saveUsers(users) {
  fs.writeFileSync(DB_PATH, JSON.stringify(users, null, 2), 'utf8');
}

var users = loadUsers();
console.log('[db] ✓ 已加载用户数据 (' + Object.keys(users).length + ' 位用户)');

// ==================== 密码哈希 ====================
var SALT_ROUNDS = 10000;
var KEY_LEN     = 64;
var DIGEST      = 'sha512';

function hashPassword(password, salt) {
  if (!salt) salt = crypto.randomBytes(16).toString('hex');
  var hash = crypto.pbkdf2Sync(password, salt, SALT_ROUNDS, KEY_LEN, DIGEST).toString('hex');
  return { hash: hash, salt: salt };
}

function verifyPassword(password, hash, salt) {
  var result = hashPassword(password, salt);
  return result.hash === hash;
}

// ==================== MIME ====================
var MIME = {
  '.html': 'text/html; charset=utf-8',
  '.css':  'text/css; charset=utf-8',
  '.js':   'application/javascript; charset=utf-8',
  '.json': 'application/json',
  '.png':  'image/png',
  '.svg':  'image/svg+xml'
};

// ==================== HTTP 静态文件 ====================
var server = http.createServer(function (req, res) {
  var urlPath = req.url === '/' ? 'index.html' : req.url;
  var filePath = path.join(__dirname, 'public', urlPath);
  var ext = path.extname(filePath);

  fs.readFile(filePath, function (err, data) {
    if (err) {
      res.writeHead(404);
      res.end('404 Not Found');
      return;
    }
    res.writeHead(200, { 'Content-Type': MIME[ext] || 'text/plain' });
    res.end(data);
  });
});

// ==================== 客户端对象 ====================
function Client(wsSocket) {
  this.ws   = wsSocket;
  this.chat = null;
  this.name = '';
  this.psk  = '';
  this.buf  = Buffer.alloc(0);
}

var clients = [];

// ==================== WebSocket 握手 ====================
server.on('upgrade', function (req, socket) {
  var key = req.headers['sec-websocket-key'];
  if (!key) { socket.destroy(); return; }

  var accept = crypto
    .createHash('sha1')
    .update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11')
    .digest('base64');

  var response =
    'HTTP/1.1 101 Switching Protocols\r\n' +
    'Upgrade: websocket\r\n' +
    'Connection: Upgrade\r\n' +
    'Sec-WebSocket-Accept: ' + accept + '\r\n' +
    '\r\n';

  socket.write(response);

  var client = new Client(socket);
  clients.push(client);
  console.log('[bridge] ✓ 浏览器已连接  (在线:' + clients.length + ')');

  socket.on('data', function (buf) {
    onWsData(client, buf);
  });

  socket.on('close', function () {
    console.log('[bridge] ✗ 浏览器断开  (在线:' + (clients.length - 1) + ')');
    onClientClose(client);
  });

  socket.on('error', function (err) {
    console.log('[bridge] ✗ WebSocket 错误:', err.message);
  });
});

// ==================== WebSocket 帧解析 ====================
function parseFrame(client, data) {
  client.buf = Buffer.concat([client.buf, data]);

  while (client.buf.length >= 2) {
    var b0     = client.buf[0];
    var opcode = b0 & 0x0f;
    var b1     = client.buf[1];
    var masked = (b1 & 0x80) !== 0;
    var paylen = b1 & 0x7f;
    var offset = 2;

    if (paylen === 126) {
      if (client.buf.length < 4) return;
      paylen = client.buf.readUInt16BE(2);
      offset = 4;
    } else if (paylen === 127) {
      if (client.buf.length < 10) return;
      paylen = client.buf.readUInt32BE(6);
      offset = 10;
    }

    var maskOff = offset;
    if (masked) offset += 4;

    var total = offset + paylen;
    if (client.buf.length < total) return;

    var payload;
    if (masked) {
      var m0 = client.buf[maskOff];
      var m1 = client.buf[maskOff + 1];
      var m2 = client.buf[maskOff + 2];
      var m3 = client.buf[maskOff + 3];
      var raw = Buffer.alloc(paylen);
      for (var i = 0; i < paylen; i++) {
        raw[i] = client.buf[offset + i] ^ [m0, m1, m2, m3][i % 4];
      }
      payload = raw.toString('utf8');
    } else {
      payload = client.buf.slice(offset, total).toString('utf8');
    }

    client.buf = client.buf.slice(total);

    if (opcode === 0x1) return payload;
    if (opcode === 0x8) {
      try { client.ws.end(); } catch (e) {}
      return null;
    }
    if (opcode === 0x9) {
      sendFrame(client, '', 0xa);
    }
  }
  return undefined;
}

// ==================== 发送 WebSocket 帧 ====================
function sendFrame(client, text, opcode) {
  if (opcode === undefined) opcode = 0x1;
  var payload = Buffer.from(text, 'utf8');
  var len = payload.length;
  var head;

  if (len < 126) {
    head = Buffer.alloc(2);
    head[0] = 0x80 | opcode;
    head[1] = len;
  } else if (len < 65536) {
    head = Buffer.alloc(4);
    head[0] = 0x80 | opcode;
    head[1] = 126;
    head.writeUInt16BE(len, 2);
  } else {
    head = Buffer.alloc(10);
    head[0] = 0x80 | opcode;
    head[1] = 127;
    head.writeUInt32BE(0, 2);
    head.writeUInt32BE(len, 6);
  }

  try {
    client.ws.write(Buffer.concat([head, payload]));
  } catch (e) {
    console.error('[bridge] ✗ 发送帧失败:', e.message);
  }
}

// ==================== 连接 C 服务端 ====================
function connectToChat(client) {
  client.chat = new net.Socket();

  client.chat.on('data', function (buf) {
    var text = buf.toString('utf8');
    var lines = text.split(/\r?\n/);
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i].trim();
      if (!line) continue;
      console.log('[bridge] C→浏览器:', line.substring(0, 60));
      if (client.ws && !client.ws.destroyed) {
        sendFrame(client, JSON.stringify({ type: 'msg', text: line }));
      }
    }
  });

  client.chat.on('close', function () {
    console.log('[bridge] ✗ C 服务端 TCP 关闭');
    if (client.ws && !client.ws.destroyed) {
      sendFrame(client, JSON.stringify({
        type: 'system', text: '与聊天室的连接已断开'
      }));
    }
  });

  client.chat.on('error', function (err) {
    console.error('[bridge] ✗ TCP 错误:', err.code, err.message);
    if (client.ws && !client.ws.destroyed) {
      sendFrame(client, JSON.stringify({
        type: 'error',
        text: '无法连接 C 服务端：' + err.message
      }));
    }
  });

  client.chat.connect(CHAT_PORT, CHAT_HOST, function () {
    console.log('[bridge] ✓ TCP 已连到 C 服务端');
    client.chat.write(client.name + '\n');
    client.chat.write(client.psk  + '\n');
    sendFrame(client, JSON.stringify({
      type: 'joined',
      text: '欢迎进入聊天室！'
    }));
  });
}

// ==================== 处理浏览器消息 ====================
function onWsData(client, data) {
  var msg = parseFrame(client, data);
  if (msg === undefined || msg === null) return;

  console.log('[bridge] ← 浏览器:', msg.substring(0, 80));

  try {
    var obj = JSON.parse(msg);

    if (obj.type === 'join') {
      client.name = String(obj.name || '匿名用户').substring(0, 16);
      client.psk  = String(obj.psk  || '').substring(0, 31);
      var isRegister = obj.register === true;
      var action = isRegister ? '注册' : '登录';

      // 密码不能为空
      if (!client.psk) {
        console.log('[bridge] ✗ 密码为空，拒绝' + action);
        sendFrame(client, JSON.stringify({
          type: 'error',
          text: isRegister ? '请设置密码' : '请输入密码'
        }));
        return;
      }

      var existing = users[client.name] || null;

      if (isRegister) {
        // ===== 注册 =====
        if (existing) {
          console.log('[db] ✗ 注册失败：用户名「' + client.name + '」已存在');
          sendFrame(client, JSON.stringify({
            type: 'error',
            text: '用户名「' + client.name + '」已被占用，请换一个'
          }));
          return;
        }

        var result = hashPassword(client.psk);
        users[client.name] = {
          hash: result.hash,
          salt: result.salt,
          created: new Date().toISOString().replace('T', ' ').substring(0, 19)
        };
        saveUsers(users);
        console.log('[db] ✓ 注册成功：「' + client.name + '」');

        connectToChat(client);

      } else {
        // ===== 登录 =====
        if (!existing) {
          console.log('[db] ✗ 登录失败：用户「' + client.name + '」不存在');
          sendFrame(client, JSON.stringify({
            type: 'error',
            text: '用户「' + client.name + '」不存在，请先注册'
          }));
          return;
        }

        if (!verifyPassword(client.psk, existing.hash, existing.salt)) {
          console.log('[db] ✗ 登录失败：密码错误');
          sendFrame(client, JSON.stringify({
            type: 'error',
            text: '密码错误，请重试'
          }));
          return;
        }

        console.log('[db] ✓ 登录成功：「' + client.name + '」');
        connectToChat(client);
      }

    } else if (obj.type === 'msg') {
      if (client.chat && !client.chat.destroyed && client.chat.writable) {
        var line = '[' + client.name + ']: ' + obj.text + '\r\n';
        client.chat.write(line);

        sendFrame(client, JSON.stringify({
          type: 'msg',
          from: 'me',
          name: client.name,
          text: obj.text
        }));
      } else {
        sendFrame(client, JSON.stringify({
          type: 'error',
          text: '尚未连接到聊天室'
        }));
      }
    }

  } catch (e) {
    console.error('[bridge] ✗ 消息处理失败:', e.message);
  }
}

function onClientClose(client) {
  if (client.chat) { try { client.chat.destroy(); } catch (e) {} }
  var idx = clients.indexOf(client);
  if (idx >= 0) clients.splice(idx, 1);
}

// ==================== 启动 ====================
server.listen(WEB_PORT, function () {
  console.log('');
  console.log('======================================================');
  console.log('  net_chat 聊天室 — Web 桥接层 v10');
  console.log('======================================================');
  console.log('  C 服务端  ← TCP :' + CHAT_PORT + ' →  bridge');
  console.log('  bridge     ← WS  :' + WEB_PORT + ' →  浏览器');
  console.log('======================================================');
  console.log('  浏览器打开: http://localhost:' + WEB_PORT);
  console.log('  确保已启动:  ./build/output/net_chat_server');
  console.log('======================================================');
  console.log('');
});
