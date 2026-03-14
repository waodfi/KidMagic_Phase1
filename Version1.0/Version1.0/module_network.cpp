#include "module_network.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>

// ============================================================================
//  内部状态
// ============================================================================
static NetStatus  httpStatus    = NET_IDLE;
static WiFiClient audioClient;
static WiFiUDP    telemetryUdp;

static String     responseBody  = "";
static uint32_t   wifiStartTime = 0;
static bool       wifiConnected = false;

// HTTP 异步状态
static bool       httpPending   = false;
static String     pendingUrl    = "";
static String     pendingBody   = "";

// ============================================================================
//  公共 API
// ============================================================================
void net_init() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    wifiStartTime = millis();
    wifiConnected = false;
    Serial.println("[NET] WiFi connecting...");
}

bool net_isConnected() {
    return wifiConnected;
}

bool net_sendCommand(const char* endpoint, const char* jsonBody) {
    if (httpPending) {
        Serial.println("[NET] HTTP busy, command dropped.");
        return false;
    }

    pendingUrl = String("http://") + SERVER_HOST + ":" + String(SERVER_PORT) + endpoint;
    pendingBody = (jsonBody != nullptr) ? String(jsonBody) : "{}";
    httpPending = true;
    httpStatus  = NET_BUSY;

    Serial.print("[NET] Queued POST -> ");
    Serial.println(pendingUrl);
    return true;
}

NetStatus net_getStatus() {
    return httpStatus;
}

const char* net_getResponseBody() {
    return responseBody.c_str();
}

void net_clearStatus() {
    httpStatus   = NET_IDLE;
    responseBody = "";
}

// ============================================================================
//  非阻塞更新
// ============================================================================
void net_update() {
    // --- WiFi 连接管理 ---
    if (!wifiConnected) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiConnected = true;
            Serial.print("[NET] WiFi connected! IP: ");
            Serial.println(WiFi.localIP());
        } else if (millis() - wifiStartTime > WIFI_TIMEOUT_MS) {
            static bool printedTimeout = false;
            if (!printedTimeout) {
                Serial.println("[NET] WiFi connection timeout! Entering offline mode.");
                printedTimeout = true;
            }
            
            // 在离线模拟模式下，瞬间完成网络请求
            if (httpPending) {
                Serial.println("[NET] Offline mode: simulating HTTP 200 OK");
                responseBody = "{\"status\":\"simulated\"}";
                httpStatus = NET_SUCCESS;
                httpPending = false;
            }
        }
        return;
    }

    // 断开连接时重新连接
    if (WiFi.status() != WL_CONNECTED) {
        wifiConnected = false;
        WiFi.reconnect();
        wifiStartTime = millis();
        Serial.println("[NET] WiFi lost, reconnecting...");
        return;
    }

    // --- 执行 HTTP 请求 ---
    if (httpPending) {
        HTTPClient http;
        http.begin(pendingUrl);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(HTTP_TIMEOUT_MS);

        int code = http.POST(pendingBody);

        if (code > 0) {
            responseBody = http.getString();
            httpStatus   = NET_SUCCESS;
            Serial.printf("[NET] HTTP %d : %s\n", code, responseBody.c_str());
        } else {
            responseBody = http.errorToString(code);
            httpStatus   = NET_ERROR;
            Serial.printf("[NET] HTTP error: %s\n", responseBody.c_str());
        }

        http.end();
        httpPending = false;
    }
}

// ============================================================================
//  音频流传输 API
// ============================================================================
bool net_startAudioStream(const char* host, uint16_t port) {
    if (!wifiConnected) {
        Serial.println("[NET] 无法启动音频流：WiFi 未连接");
        return false;
    }
    
    Serial.printf("[NET] 正在连接音频服务器 %s:%d...\n", host, port);
    if (audioClient.connect(host, port)) {
        Serial.println("[NET] 音频流连接成功！");
        return true;
    } else {
        Serial.println("[NET] 音频流连接失败！");
        return false;
    }
}

void net_sendAudioData(const uint8_t* data, size_t len) {
    if (audioClient.connected()) {
        audioClient.write(data, len);
    }
}

void net_stopAudioStream() {
    if (audioClient.connected()) {
        audioClient.stop();
    }
    Serial.println("[NET] 音频流已停止。");
}

void net_sendButtonTelemetry(const char* btnName, bool isPressed, int pressCount) {
    if (!wifiConnected) return;

    telemetryUdp.beginPacket(SERVER_HOST, TELEMETRY_PORT);
    
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"button\":\"%s\",\"pressed\":%s,\"count\":%d}", 
             btnName, isPressed ? "true" : "false", pressCount);
             
    telemetryUdp.print(payload);
    telemetryUdp.endPacket();
}
