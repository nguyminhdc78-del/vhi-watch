# VHI Watch — ESP32-C3 Smartwatch + Navigation

Đồng hồ thông minh + chỉ đường turn-by-turn trên màn hình **1.54" ST7789 240×240 SPI**, điều khiển **2 nút**, dùng **LVGL + TFT_eSPI**, build bằng **PlatformIO**.

Điện thoại (app companion Web Bluetooth) tính route bằng Google Directions API rồi đẩy từng bước rẽ qua **BLE**. Ảnh nền upload qua **WiFi AP** của đồng hồ.

---

## 1. Sơ đồ đấu dây (ESP32-C3 mini ↔ ST7789 + 2 nút)

### Màn hình ST7789 1.54" (SPI)

| ST7789 pin | Chức năng        | ESP32-C3 GPIO | Ghi chú |
|-----------|------------------|---------------|---------|
| VCC       | Nguồn 3.3V       | 3V3           | KHÔNG cắm 5V |
| GND       | Mass             | GND           | |
| SCL / SCLK| SPI clock        | **GPIO 4**    | |
| SDA / MOSI| SPI data         | **GPIO 6**    | |
| RES / RST | Reset            | **GPIO 10**   | |
| DC / RS   | Data/Command     | **GPIO 3**    | |
| CS        | Chip select      | **GPIO 7**    | Nếu module không có CS → nối GND, đặt `TFT_CS=-1` |
| BLK / BL  | Backlight        | **GPIO 5**    | PWM chỉnh độ sáng |

### 3 nút nhấn (nối GPIO → nút → GND, dùng pull-up nội)

| Nút    | ESP32-C3 GPIO | Vai trò |
|--------|---------------|---------|
| Nút A  | **GPIO 0**    | NEXT — chuyển/cuộn (giữ = cuộn ngược) |
| Nút B  | **GPIO 1**    | ENTER — chọn / mở menu |
| Nút C  | **GPIO 9**    | BACK — quay lại (thường là nút BOOT có sẵn trên board) |

> Nút C dùng GPIO 9 = chân BOOT. Đa số board C3 SuperMini đã có sẵn nút BOOT nối GPIO9 → dùng luôn, khỏi đấu thêm. Lưu ý: **đừng giữ nút C lúc cấp nguồn/reset** (sẽ vào chế độ nạp).

```
   ESP32-C3 mini                 ST7789 240x240
  ┌────────────┐                ┌──────────────┐
  │        3V3 ├────────────────┤ VCC          │
  │        GND ├────────────────┤ GND          │
  │     GPIO 4 ├────────────────┤ SCL (SCLK)   │
  │     GPIO 6 ├────────────────┤ SDA (MOSI)   │
  │     GPIO 3 ├────────────────┤ DC           │
  │    GPIO 10 ├────────────────┤ RES          │
  │     GPIO 7 ├────────────────┤ CS           │
  │     GPIO 5 ├────────────────┤ BLK          │
  │            │                └──────────────┘
  │     GPIO 0 ├───[ Nút A ]───┐
  │     GPIO 1 ├───[ Nút B ]───┤
  │        GND ├───────────────┘
  └────────────┘
```

> ⚠️ **Lưu ý chân strapping của ESP32-C3:** GPIO 2, 8, 9 là chân khởi động — **không** dùng cho nút nhấn hay tín hiệu kéo xuống lúc boot. Sơ đồ trên đã tránh các chân này.

> 🔋 **Nguồn/Pin:** dự án mẫu cấp nguồn qua USB. Muốn chạy pin Li-Po, thêm mạch sạc TP4056 + boost 3.3V, và (tuỳ chọn) chia áp pin vào 1 chân ADC để đọc `g_sys.battPercent`.

---

## 2. Build & nạp

```bash
# Cài PlatformIO (VS Code extension hoặc CLI)
pio run                 # build
pio run -t upload       # nạp firmware
pio device monitor      # xem log (115200)
```

LittleFS tự format lần đầu. Không cần nạp filesystem riêng — wallpaper được upload runtime qua WiFi.

### Nếu màn hình bị lỗi
- **Trắng xoá / không hiện:** kiểm tra lại chân DC, RST, CS và nguồn 3.3V.
- **Màu bị đảo (đỏ↔xanh):** đổi `TFT_RGB_ORDER=TFT_RGB` → `TFT_BGR` trong `platformio.ini`.
- **Màu âm bản:** bỏ comment `tft.invertDisplay(true);` trong `src/display.cpp`.
- **Nhiễu/sai khi vẽ:** đổi `LV_COLOR_16_SWAP` trong `src/lv_conf.h` (1 ↔ 0).

---

## 3. App điện thoại (companion PWA)

Thư mục `companion/` là một **web app cài được (PWA)** — chạy cả Android lẫn iPhone, miễn phí, không cần lên CH Play / App Store.

- `index.html` — UI 3 tab (Trang chủ / Chỉ đường / Ảnh nền), kết nối BLE 1 chạm
- `manifest.webmanifest` + `sw.js` + `icons/` — để cài lên màn hình chính như app thật
- `HUONG_DAN_NGUOI_DUNG.md` — hướng dẫn cho người dùng cuối

### Đưa app lên mạng (chủ dự án làm 1 lần)
1. Push toàn bộ project lên GitHub (repo **Public**).
2. **Settings → Pages → Build and deployment → Source: GitHub Actions**.
3. Workflow `.github/workflows/deploy-pages.yml` tự deploy thư mục `companion/`.
4. Lấy link `https://<user>.github.io/<repo>/` → gửi cho người dùng.

### Người dùng cài & kết nối
- **Android:** mở link bằng Chrome → "Thêm vào màn hình chính" → bấm **Kết nối** → chọn `VHI-Watch`.
- **iPhone:** cài trình duyệt **Bluefy** (miễn phí, App Store) → mở link trong Bluefy → **Kết nối**. (Safari không hỗ trợ Web Bluetooth.)

> Web Bluetooth yêu cầu **HTTPS** — GitHub Pages có sẵn HTTPS nên thoả mãn. Test nhanh trên PC: `python -m http.server` rồi mở `http://localhost:8000` bằng Chrome.

> **Ảnh nền** truyền qua WiFi (không qua BLE) vì PWA chạy HTTPS không gọi được `http://192.168.4.1` (mixed content). Tab "Ảnh nền" sẽ mở thẳng trang web tích hợp trong firmware để chọn/gửi ảnh.

---

## 4. Upload ảnh nền

1. Trên đồng hồ: Menu → **Đổi ảnh nền** (đồng hồ bật WiFi AP).
2. Điện thoại vào WiFi `VHI-Watch-Setup` (mật khẩu `12345678`).
3. Mở trình duyệt `http://192.168.4.1`, chọn ảnh → **Gửi**.
4. Ảnh được crop vuông, resize 240×240, convert **RGB565** ngay trên điện thoại rồi gửi raw (~115KB) → lưu LittleFS → hiển thị làm nền watchface.

---

## 5. Giao thức BLE

| Characteristic | UUID kết thúc | Hướng | Nội dung |
|----------------|---------------|-------|----------|
| NAV  | `...0002` | Phone→Watch (Write) | JSON gói chỉ đường |
| TIME | `...0003` | Phone→Watch (Write) | Unix epoch (giây, dạng chuỗi) |
| STATUS | `...0004` | Watch→Phone (Notify) | `{"batt":..,"nav":..}` |

Gói NAV (JSON):
```json
{ "m": 3, "d": 150, "s": "Re phai vao Le Loi", "e": "12:34", "r": 3200 }
```
- `m` maneuver: 1 thẳng · 2 trái · 3 phải · 4 chéo trái · 5 chéo phải · 6 gắt phải · 7 gắt trái · 8 quay đầu · 9 đến nơi
- `d` mét tới điểm rẽ · `r` tổng mét còn lại · `s` hướng dẫn · `e` ETA
- Dừng dẫn đường: `{ "stop": 1 }`

---

## 6. Cấu trúc code

```
src/
 ├── main.cpp        — vòng lặp chính, ghép module
 ├── config.h        — chân GPIO, UUID, hằng số
 ├── lv_conf.h       — cấu hình LVGL
 ├── display.*       — TFT_eSPI + LVGL init, backlight PWM
 ├── buttons.*       — 2 nút → keypad LVGL
 ├── app_state.*     — trạng thái dùng chung (nav, clock, sys)
 ├── storage.*       — LittleFS + nạp wallpaper
 ├── ble_nav.*       — BLE server (NimBLE)
 ├── web_upload.*    — WiFi AP + web server upload ảnh
 └── ui.*            — màn hình: watchface / menu / nav / upload
companion/
 └── index.html      — app Web Bluetooth điều khiển + dẫn đường
```

## 7. Việc còn để mở rộng (TODO)
- [ ] Đọc pin thật qua ADC (`g_sys.battPercent`)
- [ ] Màn hình **Cài đặt** (độ sáng, đổi mặt đồng hồ) và **Thông tin**
- [ ] Vẽ mũi tên rẽ bằng ảnh/canvas thay cho ký hiệu LVGL
- [ ] Sleep/wake để tiết kiệm pin (light-sleep khi không thao tác)
- [ ] App Flutter cho iOS
