#ifndef MODULE_NETWORK_H
#define MODULE_NETWORK_H

#include "Config.h"

/// HTTP 响应状态
enum NetStatus {
    NET_IDLE,
    NET_BUSY,
    NET_SUCCESS,
    NET_ERROR
};

/// 初始化 WiFi 连接 (非阻塞，需调用 net_update 驱动)。
void net_init();

/// 成功连接 WiFi 后返回 true。
bool net_isConnected();

/// 发送 HTTP POST 请求到指定的 API 端点。
/// @param endpoint  例如 "/api/capture"
/// @param jsonBody  可选的 JSON 字符串，或 nullptr
/// @return 请求成功加入队列时返回 true
bool net_sendCommand(const char* endpoint, const char* jsonBody = nullptr);

/// 获取上一次 HTTP 请求的状态。
NetStatus net_getStatus();

/// 获取上一次完成的 HTTP 请求响应体。
const char* net_getResponseBody();

/// 将状态清除恢复为 NET_IDLE (使用完结果后调用)。
void net_clearStatus();

/// 每次 loop() 循环调用以驱动 WiFi 连接和 HTTP 状态机。
void net_update();

/// 启动音频 TCP 传输流
bool net_startAudioStream(const char* host, uint16_t port);

/// 发送音频数据
void net_sendAudioData(const uint8_t* data, size_t len);

/// 停止音频 TCP 传输流
void net_stopAudioStream();

/// 发送按键遥测数据 (UDP)
void net_sendButtonTelemetry(const char* btnName, bool isPressed, int pressCount);

#endif // MODULE_NETWORK_H
