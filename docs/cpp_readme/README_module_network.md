# module_network.cpp 讲解文档

对应源码：
- src/module_network.cpp

对应头文件：
- src/module_network.h

依赖：
- include/Config.h（WiFi 账号、服务器地址、超时）
- WiFi 库
- HTTPClient 库

---

## 1. 模块职责

这个模块负责：
1. 连接 WiFi
2. 发送 HTTP POST 请求
3. 保存最近一次请求结果
4. 通过状态机让主循环非阻塞地驱动网络流程

注意：当前 main.cpp 主要在做硬件调试，这个模块处于可选状态。

---

## 2. 内部状态变量

1. httpStatus
- 当前网络状态：NET_IDLE / NET_BUSY / NET_SUCCESS / NET_ERROR

2. responseBody
- 最近一次 HTTP 返回内容

3. wifiStartTime
- 用于连接超时判断

4. wifiConnected
- 是否已连上 WiFi

5. httpPending
- 是否有待执行请求

6. pendingUrl / pendingBody
- 排队请求的 URL 和 JSON 请求体

---

## 3. net_init

做的事情：
1. WiFi.mode(WIFI_STA)
2. WiFi.begin(SSID, PASSWORD)
3. 记录开始时间
4. 先标记未连接

这个函数不会阻塞等待成功，真正连接过程在 net_update 中推进。

---

## 4. net_sendCommand

输入 endpoint 和 jsonBody，作用是“入队请求”：
1. 如果当前已有请求在处理，直接丢弃并返回 false
2. 拼接 URL：http://SERVER_HOST:SERVER_PORT + endpoint
3. 记录待发送 body
4. 设置 httpPending=true，状态转 NET_BUSY

这里采用单请求队列模型，逻辑简单，适合入门阶段。

---

## 5. net_update 状态机

每次 loop 调用一次，流程：

1. 若未连接 WiFi
- 连上则打印 IP
- 超时则进入离线模拟模式

2. 离线模拟模式
- 如果有待请求，直接返回模拟成功 JSON
- 用于无网络也能跑通上层逻辑

3. 若已连接但中途断线
- 触发重连

4. 若有待发送 HTTP
- 创建 HTTPClient
- POST 请求
- 保存响应体和状态
- 清 pending 标志

---

## 6. 头文件 module_network.h 解读

### NetStatus 枚举
1. NET_IDLE：空闲
2. NET_BUSY：处理中
3. NET_SUCCESS：成功
4. NET_ERROR：失败

### 导出 API
1. net_init
2. net_isConnected
3. net_sendCommand
4. net_getStatus
5. net_getResponseBody
6. net_clearStatus
7. net_update

典型调用顺序：
1. setup 调 net_init
2. loop 每帧调 net_update
3. 事件发生时调 net_sendCommand
4. 轮询 net_getStatus 获取结果
5. 处理完后 net_clearStatus

---

## 7. 常见修改点

1. 改服务器地址
- 修改 include/Config.h 的 SERVER_HOST 和 SERVER_PORT

2. 改 API 路径
- 修改 Config.h 中 API_CAPTURE 等宏

3. 增加认证头
- 在发送请求处 addHeader 增加 token

4. 支持多请求队列
- 把单 pending 变量改成队列容器

---

## 8. 风险与改进建议

1. 当前 HTTP 仍是同步 POST
- 对网络很慢的场景可能卡顿
- 后续可改成任务线程或异步客户端

2. responseBody 仅保存最近一次
- 若要追踪历史，需要日志队列

3. 离线模拟目前固定返回 simulated
- 可按 endpoint 返回不同假数据，便于联调
