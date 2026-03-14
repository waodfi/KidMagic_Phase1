import serial
import wave
import time

# ----------------- 配置区 -----------------
# ！！！请替换成你在 Arduino IDE 中使用的 COM 口！！！
COM_PORT = 'COM10'   
BAUD_RATE = 915200  # 与 Arduino 端 Serial.begin() 中的波特率一致
OUTPUT_FILENAME = "my_record.wav"

# 根据你 module_mic.cpp 中的 I2S 配置
CHANNELS = 1          # 单声道 MONO
SAMPLE_RATE = 16000   # 16000 Hz 采样率
SAMPLE_WIDTH = 4      # 32BIT 数据深度
# ------------------------------------------

try:
    ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=0.5)
    print(f"正在连接串口 {COM_PORT}...")
except Exception as e:
    print(f"无法打开串口: {e}")
    print("提示：请确认你的 Arduino IDE '串口监视器' 和 '绘图器' 是否已经关闭！")
    exit()

print("✅ 连接成功！请在设备上按下【RECORD】物理按键开始录音...")
ser.reset_input_buffer()

frames = bytearray()
is_recording = False

try:
    while True:
        # 当串口有数据时（设备按下了RECORD按键）开始接收
        if ser.in_waiting > 0:
            if not is_recording:
                print("🎙️ 检测到音频数据，开始录制... (录完请按 Ctrl+C 停止脚本并保存文件)")
                is_recording = True
            
            data = ser.read(ser.in_waiting)
            frames.extend(data)
            
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\n⏹️ 脚本已手动停止。正在保存 WAV 文件...")

finally:
    ser.close()
    if len(frames) > 0:
        with wave.open(OUTPUT_FILENAME, 'wb') as wf:
            wf.setnchannels(CHANNELS)
            wf.setsampwidth(SAMPLE_WIDTH)
            wf.setframerate(SAMPLE_RATE)
            wf.writeframes(frames)
        print(f"🎉 文件已成功保存至当前目录: {OUTPUT_FILENAME}")
    else:
        print("⚠️ 没有接收到任何音频数据，本次未生成文件。")