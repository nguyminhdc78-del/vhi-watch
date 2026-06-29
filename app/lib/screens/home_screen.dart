import 'package:flutter/material.dart';
import '../ble_service.dart';
import '../theme.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  @override
  Widget build(BuildContext context) {
    final ble = BleService.I;
    return ListenableBuilder(
      listenable: ble,
      builder: (context, _) {
        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    const Text('Kết nối đồng hồ',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                    const SizedBox(height: 8),
                    Text(ble.status, style: TextStyle(color: Colors.grey[400])),
                    const SizedBox(height: 12),
                    if (!ble.connected)
                      FilledButton.icon(
                        onPressed: ble.busy ? null : ble.connect,
                        icon: ble.busy
                            ? const SizedBox(
                                width: 18, height: 18,
                                child: CircularProgressIndicator(strokeWidth: 2))
                            : const Icon(Icons.bluetooth_searching),
                        label: Text(ble.busy ? 'Đang xử lý...' : 'Kết nối ngay'),
                      )
                    else
                      FilledButton.icon(
                        style: FilledButton.styleFrom(backgroundColor: AppColors.danger),
                        onPressed: ble.disconnect,
                        icon: const Icon(Icons.bluetooth_disabled),
                        label: const Text('Ngắt kết nối'),
                      ),
                  ],
                ),
              ),
            ),
            if (ble.connected)
              Card(
                child: Padding(
                  padding: const EdgeInsets.all(16),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: [
                      const Text('Trạng thái',
                          style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                      const SizedBox(height: 8),
                      _row('Thiết bị', ble.deviceName ?? '—'),
                      _row('Pin', '${ble.battery}%'),
                      _row('Đang dẫn đường', ble.navActive ? 'Có' : 'Không'),
                      _row('Đồng bộ giờ',
                          ble.lastSync == null
                              ? '—'
                              : '${ble.lastSync!.hour.toString().padLeft(2, '0')}:${ble.lastSync!.minute.toString().padLeft(2, '0')}'),
                      const SizedBox(height: 12),
                      OutlinedButton.icon(
                        onPressed: ble.syncTime,
                        icon: const Icon(Icons.access_time),
                        label: const Text('Đồng bộ giờ'),
                      ),
                    ],
                  ),
                ),
              ),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('Hướng dẫn nhanh',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                    const SizedBox(height: 8),
                    Text(
                      '1. Bật đồng hồ, để gần điện thoại.\n'
                      '2. Bật Bluetooth + Vị trí trên điện thoại.\n'
                      '3. Nhấn "Kết nối ngay" và chọn VHI-Watch.',
                      style: TextStyle(color: Colors.grey[400], height: 1.5),
                    ),
                  ],
                ),
              ),
            ),
          ],
        );
      },
    );
  }

  Widget _row(String k, String v) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 6),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [
            Text(k, style: const TextStyle(color: Colors.grey)),
            Text(v, style: const TextStyle(fontWeight: FontWeight.w500)),
          ],
        ),
      );
}
