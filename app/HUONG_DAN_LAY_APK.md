# Cách lấy file APK về điện thoại Android

Máy bạn chưa cài Flutter/Android SDK, nên ta để **GitHub build APK miễn phí trên mây**, rồi tải file `.apk` về điện thoại cài. Làm 1 lần, các lần sau chỉ cần `git push` là có APK mới.

---

## Bước 1 — Tạo tài khoản & repo GitHub
1. Đăng ký tài khoản tại https://github.com (nếu chưa có).
2. Bấm **New repository** → đặt tên (vd `vhi-watch`) → chọn **Public** → **Create repository**.

## Bước 2 — Đẩy code lên GitHub
Mở **Git Bash** / terminal tại thư mục `d:\minhduc\esp32c3_smartwatch` rồi chạy (thay `<user>` và tên repo cho đúng):

```bash
cd /d/minhduc/esp32c3_smartwatch
git init
git add .
git commit -m "VHI Watch: firmware + app Android"
git branch -M main
git remote add origin https://github.com/<user>/vhi-watch.git
git push -u origin main
```

> Lần đầu push, GitHub hỏi đăng nhập → đăng nhập bằng trình duyệt hiện ra.

## Bước 3 — Đợi GitHub build APK
1. Vào repo trên web → tab **Actions**.
2. Sẽ thấy job **"Build Android APK"** đang chạy (~5–10 phút lần đầu).
3. Chạy xong có dấu ✓ xanh.

> Nếu báo lỗi tạo Release: vào **Settings → Actions → General → Workflow permissions** → chọn **Read and write permissions** → Save → vào Actions chạy lại (Re-run).

## Bước 4 — Tải APK về điện thoại
**Cách A (dễ nhất):** Trên điện thoại, mở trình duyệt vào:
```
https://github.com/<user>/vhi-watch/releases
```
Bấm vào bản **"VHI Watch APK (bản mới nhất)"** → tải file **vhi-watch.apk**.

**Cách B:** Tab **Actions** → mở lần chạy mới nhất → mục **Artifacts** → tải **vhi-watch-apk** (file zip, giải nén ra apk).

## Bước 5 — Cài đặt
1. Mở file `vhi-watch.apk` vừa tải trên điện thoại.
2. Android hỏi "cài từ nguồn không xác định" → bấm **Cho phép / Settings** → bật cho trình duyệt → quay lại **Cài đặt**.
3. Mở app **VHI Watch** → bật Bluetooth + Vị trí → **Kết nối ngay** → chọn VHI-Watch.

---

## Lần sau cập nhật app
Chỉ cần sửa code rồi:
```bash
git add .
git commit -m "cap nhat"
git push
```
GitHub tự build lại, vào Releases tải APK mới.

---

## Không muốn dùng GitHub?
Cách duy nhất khác là **tự cài Flutter SDK + Android Studio** (~vài GB, cần JDK 17) trên máy rồi chạy:
```bash
cd app
flutter create --platforms=android --org com.vhitek --project-name vhi_watch .
copy android_manifest_template.xml android\app\src\main\AndroidManifest.xml
flutter build apk --release
```
File APK ở: `build/app/outputs/flutter-apk/app-release.apk`. Cách này chủ động nhưng setup lâu.
