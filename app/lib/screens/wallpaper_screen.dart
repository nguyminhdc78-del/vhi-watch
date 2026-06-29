import 'dart:typed_data';
import 'package:flutter/material.dart';
import 'package:image/image.dart' as img;
import 'package:image_picker/image_picker.dart';
import '../ble_service.dart';

// ============================================================
//  Doi anh nen: chon anh -> crop vuong + resize 240x240 ->
//  convert RGB565 -> POST len http://192.168.4.1/upload (WiFi AP cua dong ho)
//  (App native nen KHONG vuong mixed-content nhu PWA)
// ============================================================
class WallpaperScreen extends StatefulWidget {
  const WallpaperScreen({super.key});
  @override
  State<WallpaperScreen> createState() => _WallpaperScreenState();
}

class _WallpaperScreenState extends State<WallpaperScreen> {
  Uint8List? _preview; // PNG 240x240 de hien thi
  Uint8List? _rgb565; // du lieu gui di
  String _stat = '';
  bool _sending = false;

  Future<void> _pick() async {
    final x = await ImagePicker().pickImage(source: ImageSource.gallery);
    if (x == null) return;
    setState(() => _stat = 'Đang xử lý ảnh...');
    final bytes = await x.readAsBytes();
    final im = img.decodeImage(bytes);
    if (im == null) {
      setState(() => _stat = 'Không đọc được ảnh');
      return;
    }
    final sq = img.copyResizeCropSquare(im, size: 240);

    // RGB565 little-endian, khop firmware
    final out = Uint8List(240 * 240 * 2);
    var k = 0;
    for (var y = 0; y < 240; y++) {
      for (var x = 0; x < 240; x++) {
        final p = sq.getPixel(x, y);
        final r = p.r.toInt(), g = p.g.toInt(), b = p.b.toInt();
        final v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        out[k++] = v & 0xff;
        out[k++] = (v >> 8) & 0xff;
      }
    }

    setState(() {
      _preview = img.encodePng(sq);
      _rgb565 = out;
      _stat = 'Đã sẵn sàng. Nhấn "Gửi vào đồng hồ".';
    });
  }

  Future<void> _send() async {
    if (_rgb565 == null) return;
    if (!BleService.I.canSendWallpaper) {
      setState(() => _stat =
          'Chưa kết nối đồng hồ. Vào tab "Trang chủ" → Kết nối trước.');
      return;
    }
    setState(() {
      _sending = true;
      _stat = 'Đang gửi qua Bluetooth... 0%';
    });
    try {
      await BleService.I.sendWallpaper(_rgb565!, onProgress: (p) {
        setState(() => _stat = 'Đang gửi... ${(p * 100).toStringAsFixed(0)}%');
      });
      setState(() => _stat = '✅ Đã gửi xong! Đồng hồ sẽ hiện ảnh sau vài giây.');
    } catch (e) {
      setState(() => _stat = 'Lỗi gửi: $e');
    } finally {
      setState(() => _sending = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('Đổi ảnh nền đồng hồ',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                const SizedBox(height: 8),
                Text(
                  '1. Kết nối đồng hồ qua Bluetooth (tab Trang chủ).\n'
                  '2. Chọn ảnh bên dưới.\n'
                  '3. Nhấn "Gửi vào đồng hồ" — chờ ~30 giây.',
                  style: TextStyle(color: Colors.grey[400], height: 1.5, fontSize: 13),
                ),
                const SizedBox(height: 14),
                Center(
                  child: Container(
                    width: 200,
                    height: 200,
                    decoration: BoxDecoration(
                      color: const Color(0xFF11161E),
                      borderRadius: BorderRadius.circular(12),
                      border: Border.all(color: const Color(0xFF2A3240)),
                    ),
                    clipBehavior: Clip.antiAlias,
                    child: _preview == null
                        ? const Center(
                            child: Icon(Icons.image_outlined,
                                size: 48, color: Colors.grey))
                        : Image.memory(_preview!, fit: BoxFit.cover),
                  ),
                ),
                const SizedBox(height: 14),
                FilledButton.icon(
                  onPressed: _pick,
                  icon: const Icon(Icons.photo_library),
                  label: const Text('Chọn ảnh'),
                ),
                const SizedBox(height: 8),
                FilledButton.icon(
                  onPressed: (_rgb565 == null || _sending) ? null : _send,
                  icon: _sending
                      ? const SizedBox(
                          width: 18, height: 18,
                          child: CircularProgressIndicator(strokeWidth: 2))
                      : const Icon(Icons.upload),
                  label: const Text('Gửi vào đồng hồ'),
                ),
                if (_stat.isNotEmpty) ...[
                  const SizedBox(height: 12),
                  Text(_stat, style: TextStyle(color: Colors.grey[300])),
                ],
              ],
            ),
          ),
        ),
      ],
    );
  }
}
