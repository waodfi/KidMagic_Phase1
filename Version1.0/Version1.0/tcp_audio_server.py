import socket
import wave
import threading
import json

HOST = '0.0.0.0'        # 监听所有可用的网络接口
PORT = 8085             # 与 硬件的端口 保持一致
TELEMETRY_PORT = 8086   # 按键遥测端口
OUTPUT_FILENAME = "wifi_record.wav"
CHANNELS = 1            # 单声道 MONO
SAMPLE_RATE = 16000     #采样率16000Hz
SAMPLE_WIDTH = 2  # 16-bit

# --- UDP 遥测接收线程 ---
def udp_telemetry_listener():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_sock:
        udp_sock.bind((HOST, TELEMETRY_PORT))
        print(f"🚀 UDP 按键遥测服务已启动 (端口: {TELEMETRY_PORT})")
        while True:
            try:
                data, addr = udp_sock.recvfrom(1024)
                payload = data.decode('utf-8')
                try:
                    js = json.loads(payload)
                    pressed_str = "按下了" if js.get("pressed") else "松开了"
                    print(f"\n🕹️ [按键状态] 来自 {addr[0]}: {js.get('button')} {pressed_str}, 累计次数: {js.get('count')}")
                except json.JSONDecodeError:
                    print(f"\n🕹️ [按键状态] 来自 {addr[0]}: {payload}")
            except Exception as e:
                print(f"UDP 接收异常: {e}")

telemetry_thread = threading.Thread(target=udp_telemetry_listener, daemon=True)
telemetry_thread.start()

print(f"📡 正在启动 WiFi 音频接收服务器 (端口: {PORT})...")
print("⚠️ 请确保你的电脑和 ESP32 连在同一个局域网，并且 Config.h 中的 Wi-Fi 账号密码和 SERVER_HOST (你电脑的局域网IP) 配置正确！")
print(f"🔎 当前监听配置: TCP={PORT}, UDP={TELEMETRY_PORT}")
print("🔎 若 ESP32 仍无响应，请确认 Config.h 的 AUDIO_STREAM_PORT / TELEMETRY_PORT 与此一致")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen(1)
    
    while True:
        print("\n⏳ 等待 ESP32 按下录音键连接...")
        conn, addr = s.accept()
        with conn:
            print(f"✅ ESP32 已连接: {addr[0]}，正在接收录音中... (松开按键结束)")
            frames = bytearray()
            
            try:
                while True:
                    data = conn.recv(4096)
                    if not data:
                        # ESP32 停止录音并断开连接
                        break
                    frames.extend(data)
            except Exception as e:
                print(f"连接断开或异常: {e}")
            
            print("⏹️ 接收结束，正在保存文件...")
            
            if len(frames) > 0:
                with wave.open(OUTPUT_FILENAME, 'wb') as wf:
                    wf.setnchannels(CHANNELS)
                    wf.setsampwidth(SAMPLE_WIDTH)
                    wf.setframerate(SAMPLE_RATE)
                    wf.writeframes(frames)
                print(f"🎉 录音保存成功: {OUTPUT_FILENAME} (共 {(len(frames)/1024):.1f} KB)")
            else:
                print("⚠️ 未收到任何有效音频数据！")