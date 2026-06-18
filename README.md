# 🎮 AeroTX-OS (Dự án TX01)

**AeroTX-OS** là hệ điều hành mã nguồn mở dành riêng cho tay cầm điều khiển (RC Transmitter) tự thiết kế, chạy trên vi điều khiển **ESP32**. Dự án hướng đến việc xây dựng một hệ thống truyền nhận sóng siêu trễ (Ultra-low latency) kết hợp cùng giao diện người dùng chuyên nghiệp.

## 🚀 Đặc điểm kỹ thuật nổi bật
- **Dual-core Architecture (FreeRTOS):**
  - **Core 0:** Dành riêng (Dedicated) cho hệ thống Radio (CRSF/ELRS) đạt tần số quét 250Hz không gián đoạn.
  - **Core 1:** Chạy hệ thống đồ họa UI (LVGL) và quản lý thẻ nhớ, đảm bảo không bao giờ lock tài nguyên sóng.
- **Flight Modes Framework:** Chuyển đổi mượt mà giữa các chế độ Bay (Flight), Cài đặt (Menu), và Giả lập (Simulator BLE HID).
- **Pro Features:** Trộn kênh (Mixer & Logic), cấu hình điểm chết (Expo/Rates), và bảng điều khiển Telemetry theo thời gian thực.

## 🛠️ Yêu cầu Build (Dành cho Developer)
Dự án được viết bằng C/C++ và biên dịch trên nền tảng **ESP-IDF v5.x**.

```bash
# Clone dự án
git clone https://github.com/vinhphannn/AeroTX-OS.git
cd AeroTX-OS

# Cấu hình môi trường ESP-IDF (nếu đã cài đặt)
. $HOME/esp/esp-idf/export.sh

# Build & Flash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```
