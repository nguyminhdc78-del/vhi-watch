import 'package:flutter/material.dart';
import '../ble_service.dart';

// ============================================================
//  Danh ba nhanh: chon nguoi hay goi -> dong bo xuong dong ho.
//  Tren dong ho vao menu "Goi dien" chon nguoi + bam goi.
// ============================================================
class QuickCallScreen extends StatelessWidget {
  const QuickCallScreen({super.key});

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
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('Danh bạ nhanh',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      'Chọn người hay gọi. Trên đồng hồ vào menu "Gọi điện" → chọn → bấm B để gọi. Tối đa ${BleService.maxFavorites} người.',
                      style: TextStyle(color: Colors.grey[400], fontSize: 13),
                    ),
                    const SizedBox(height: 12),
                    Row(
                      children: [
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: ble.favorites.length >= BleService.maxFavorites
                                ? null
                                : ble.pickFromContacts,
                            icon: const Icon(Icons.contacts),
                            label: const Text('Từ danh bạ'),
                          ),
                        ),
                        const SizedBox(width: 8),
                        Expanded(
                          child: OutlinedButton.icon(
                            onPressed: ble.favorites.length >= BleService.maxFavorites
                                ? null
                                : () => _addManual(context),
                            icon: const Icon(Icons.keyboard),
                            label: const Text('Nhập tay'),
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            if (ble.favorites.isEmpty)
              Padding(
                padding: const EdgeInsets.all(24),
                child: Text('Chưa có ai. Thêm bằng 2 nút trên.',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.grey[500])),
              )
            else
              ...List.generate(ble.favorites.length, (i) {
                final f = ble.favorites[i];
                return Card(
                  child: ListTile(
                    leading: const CircleAvatar(child: Icon(Icons.person)),
                    title: Text(f['name'] ?? ''),
                    subtitle: Text(f['number'] ?? ''),
                    trailing: IconButton(
                      icon: const Icon(Icons.delete_outline, color: Colors.redAccent),
                      onPressed: () => ble.removeFavorite(i),
                    ),
                  ),
                );
              }),
            if (!ble.connected)
              Padding(
                padding: const EdgeInsets.all(12),
                child: Text('Kết nối đồng hồ để đồng bộ danh bạ.',
                    textAlign: TextAlign.center,
                    style: TextStyle(color: Colors.orange[300], fontSize: 13)),
              ),
          ],
        );
      },
    );
  }

  Future<void> _addManual(BuildContext context) async {
    final nameC = TextEditingController();
    final numC = TextEditingController();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Thêm liên hệ'),
        content: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            TextField(
              controller: nameC,
              decoration: const InputDecoration(labelText: 'Tên (VD: Mẹ)'),
            ),
            TextField(
              controller: numC,
              keyboardType: TextInputType.phone,
              decoration: const InputDecoration(labelText: 'Số điện thoại'),
            ),
          ],
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Huỷ')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Thêm')),
        ],
      ),
    );
    if (ok == true) {
      await BleService.I.addFavorite(nameC.text, numC.text);
    }
  }
}
