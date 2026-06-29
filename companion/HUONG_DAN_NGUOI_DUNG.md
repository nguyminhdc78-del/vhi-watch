# VHI Watch — Hướng dẫn cài & kết nối (cho người dùng)

App điều khiển đồng hồ là một **web app cài được** (PWA). Không cần vào CH Play / App Store, không tốn phí.

> Link app (chủ dự án điền vào sau khi bật GitHub Pages):
> **https://<tên-github>.github.io/<tên-repo>/**

---

## 📱 ANDROID (dễ nhất)

1. Mở link app bằng **Chrome**.
2. Bấm menu **⋮ → "Thêm vào màn hình chính" / "Cài đặt ứng dụng"**.
   (hoặc bấm nút **"Cài vào màn hình chính"** ngay trong app)
3. Mở app từ icon vừa tạo → bấm **🔗 Kết nối ngay** → chọn **VHI-Watch**.
4. Xong! Giờ tự đồng bộ, vào tab **Chỉ đường** / **Ảnh nền** để dùng.

---

## 🍎 IPHONE / IPAD

Safari của Apple **không** hỗ trợ Bluetooth web, nên cần một trình duyệt chuyên dụng:

1. Vào **App Store**, cài app **Bluefy – Web BLE Browser** (miễn phí).
2. Mở **Bluefy**, dán link app vào thanh địa chỉ.
3. Bấm **🔗 Kết nối ngay** → chọn **VHI-Watch**.
4. (Tuỳ chọn) Trong Safari có thể "Thêm vào màn hình chính" để có icon, nhưng phần kết nối Bluetooth phải mở trong **Bluefy**.

---

## 🔋 Trước khi kết nối
- Bật đồng hồ, để gần điện thoại.
- Bật **Bluetooth** điện thoại. Android cần bật thêm **Vị trí (Location)** để quét BLE (yêu cầu của hệ điều hành, app không lấy vị trí của bạn ở bước này).

## 🧭 Dẫn đường
- Tab **Chỉ đường** → nhập **Google Maps API Key** + **điểm đến** → **Bắt đầu**. App dùng GPS điện thoại để đẩy từng bước rẽ xuống đồng hồ.
- Chưa có API key? Dùng phần **"Gửi lệnh rẽ thủ công"** để thử.

## 🖼️ Đổi ảnh nền
1. Trên đồng hồ: Menu → **Đổi ảnh nền**.
2. Điện thoại vào WiFi **VHI-Watch-Setup** (mật khẩu **12345678**).
3. Trong app, tab **Ảnh nền** → **Mở trang chọn ảnh** → chọn ảnh → gửi.

---

## ❓ Hay gặp
- **Không thấy VHI-Watch khi quét:** đảm bảo đồng hồ đang bật, Bluetooth + Vị trí (Android) đang bật, và đồng hồ chưa kết nối với điện thoại khác.
- **iPhone bấm Kết nối không hiện gì:** bạn đang mở bằng Safari — hãy mở trong **Bluefy**.
- **Mở ảnh nền báo lỗi:** kiểm tra đã vào đúng WiFi **VHI-Watch-Setup** chưa.
