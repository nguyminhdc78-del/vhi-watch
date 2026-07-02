import 'package:flutter/material.dart';
import '../ble_service.dart';
import '../smartwatch_service.dart';
import '../theme.dart';

class HomeScreen extends StatelessWidget {
  const HomeScreen({super.key});

  // Bang mau [R,G,B] cho so gio / chu tren dong ho
  static const List<List<int>> _palette = [
    [255, 255, 255], [255, 60, 60], [255, 160, 40], [255, 220, 40],
    [60, 220, 100], [60, 220, 220], [60, 140, 255], [180, 100, 255], [255, 100, 180],
  ];

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
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('Thông báo & Nhạc',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                    const SizedBox(height: 8),
                    Text(
                      'Cho phép app đọc thông báo để hiện tin nhắn / bài hát đang phát lên đồng hồ. Bấm nút dưới rồi bật "VHI Watch" trong danh sách.',
                      style: TextStyle(color: Colors.grey[400], height: 1.4),
                    ),
                    const SizedBox(height: 12),
                    OutlinedButton.icon(
                      onPressed: () => SmartwatchService.I.requestAccess(),
                      icon: const Icon(Icons.notifications_active),
                      label: const Text('Bật quyền đọc thông báo'),
                    ),
                  ],
                ),
              ),
            ),
            const _QuickReplyCard(),
            const _WeatherCard(),
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text('Màu giờ & chữ',
                        style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                    const SizedBox(height: 4),
                    Text(
                      ble.connected
                          ? 'Chạm 1 màu để đổi màu số giờ trên đồng hồ.'
                          : 'Kết nối đồng hồ trước rồi chọn màu.',
                      style: TextStyle(color: Colors.grey[400], fontSize: 13),
                    ),
                    const SizedBox(height: 12),
                    Wrap(
                      spacing: 14,
                      runSpacing: 14,
                      children: [
                        for (final c in _palette)
                          GestureDetector(
                            onTap: () => ble.sendColor(c[0], c[1], c[2]),
                            child: Container(
                              width: 42,
                              height: 42,
                              decoration: BoxDecoration(
                                color: Color.fromARGB(255, c[0], c[1], c[2]),
                                shape: BoxShape.circle,
                                border: Border.all(color: Colors.white24, width: 2),
                              ),
                            ),
                          ),
                      ],
                    ),
                  ],
                ),
              ),
            ),
            const _WatchfaceCard(),
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

// Thẻ soạn câu trả lời nhanh — chọn trên đồng hồ để rep tin Zalo/Mess
class _QuickReplyCard extends StatelessWidget {
  const _QuickReplyCard();

  @override
  Widget build(BuildContext context) {
    final ble = BleService.I;
    return ListenableBuilder(
      listenable: ble,
      builder: (context, _) => Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text('Trả lời nhanh',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
              const SizedBox(height: 4),
              Text(
                'Soạn sẵn câu trả lời. Khi có tin Zalo/Mess, trên đồng hồ bấm B → chọn câu → gửi. Tối đa ${BleService.maxReplies} câu.',
                style: TextStyle(color: Colors.grey[400], fontSize: 13),
              ),
              const SizedBox(height: 8),
              for (int i = 0; i < ble.quickReplies.length; i++)
                ListTile(
                  dense: true,
                  contentPadding: EdgeInsets.zero,
                  leading: const Icon(Icons.chat_bubble_outline, size: 20),
                  title: Text(ble.quickReplies[i]),
                  trailing: IconButton(
                    icon: const Icon(Icons.delete_outline, color: Colors.redAccent),
                    onPressed: () => ble.removeReply(i),
                  ),
                ),
              const SizedBox(height: 4),
              OutlinedButton.icon(
                onPressed: ble.quickReplies.length >= BleService.maxReplies
                    ? null
                    : () => _addReply(context),
                icon: const Icon(Icons.add),
                label: const Text('Thêm câu trả lời'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Future<void> _addReply(BuildContext context) async {
    final c = TextEditingController();
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Câu trả lời'),
        content: TextField(
          controller: c,
          autofocus: true,
          decoration: const InputDecoration(hintText: 'VD: Đang bận, gọi lại sau'),
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Huỷ')),
          FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Thêm')),
        ],
      ),
    );
    if (ok == true) await BleService.I.addReply(c.text);
  }
}

// Thẻ tuỳ chỉnh giao diện giờ (vị trí + cỡ) — cho khỏi che ảnh nền
class _WatchfaceCard extends StatelessWidget {
  const _WatchfaceCard();
  @override
  Widget build(BuildContext context) {
    final ble = BleService.I;
    return ListenableBuilder(
      listenable: ble,
      builder: (context, _) => Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text('Giao diện giờ (mặt Số lớn)',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
              const SizedBox(height: 4),
              Text('Chỉnh vị trí & cỡ giờ để không che ảnh nền.',
                  style: TextStyle(color: Colors.grey[400], fontSize: 13)),
              const SizedBox(height: 12),
              const Text('Vị trí', style: TextStyle(color: Colors.grey)),
              const SizedBox(height: 6),
              Wrap(spacing: 8, children: [
                for (final p in const [[0, 'Trên'], [1, 'Giữa'], [2, 'Dưới']])
                  ChoiceChip(
                    label: Text(p[1] as String),
                    selected: ble.wfPos == p[0],
                    onSelected: (_) => ble.setWfLayout(pos: p[0] as int),
                  ),
              ]),
              const SizedBox(height: 12),
              const Text('Cỡ giờ', style: TextStyle(color: Colors.grey)),
              const SizedBox(height: 6),
              Wrap(spacing: 8, children: [
                for (final s in const [[3, 'Nhỏ'], [4, 'Vừa'], [5, 'To'], [6, 'Rất to']])
                  ChoiceChip(
                    label: Text(s[1] as String),
                    selected: ble.wfSize == s[0],
                    onSelected: (_) => ble.setWfLayout(size: s[0] as int),
                  ),
              ]),
              const Divider(height: 24),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  const Text('Hiện ngày', style: TextStyle(fontWeight: FontWeight.w500)),
                  Switch(
                    value: ble.dateShow,
                    onChanged: (v) => ble.setWfLayout(dShow: v),
                  ),
                ],
              ),
              if (ble.dateShow) ...[
                const SizedBox(height: 6),
                const Text('Cỡ ngày', style: TextStyle(color: Colors.grey)),
                const SizedBox(height: 6),
                Wrap(spacing: 8, children: [
                  for (final s in const [[1, 'Nhỏ'], [2, 'Vừa'], [3, 'To']])
                    ChoiceChip(
                      label: Text(s[1] as String),
                      selected: ble.dateSize == s[0],
                      onSelected: (_) => ble.setWfLayout(dSize: s[0] as int),
                    ),
                ]),
                const SizedBox(height: 12),
                const Text('Màu ngày', style: TextStyle(color: Colors.grey)),
                const SizedBox(height: 6),
                Wrap(spacing: 12, runSpacing: 12, children: [
                  for (final c in HomeScreen._palette)
                    GestureDetector(
                      onTap: () => ble.setWfLayout(dColor: c),
                      child: Container(
                        width: 34, height: 34,
                        decoration: BoxDecoration(
                          color: Color.fromARGB(255, c[0], c[1], c[2]),
                          shape: BoxShape.circle,
                          border: Border.all(color: Colors.white24, width: 2),
                        ),
                      ),
                    ),
                ]),
              ],
            ],
          ),
        ),
      ),
    );
  }
}

// Thẻ thời tiết: hiện nhiệt độ hiện tại + đổi thành phố
class _WeatherCard extends StatefulWidget {
  const _WeatherCard();
  @override
  State<_WeatherCard> createState() => _WeatherCardState();
}

class _WeatherCardState extends State<_WeatherCard> {
  late final TextEditingController _c;
  @override
  void initState() {
    super.initState();
    _c = TextEditingController(text: BleService.I.weatherCity);
  }

  @override
  void dispose() {
    _c.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final ble = BleService.I;
    return ListenableBuilder(
      listenable: ble,
      builder: (context, _) => Card(
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              const Text('Thời tiết',
                  style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
              const SizedBox(height: 8),
              Text(
                ble.weatherOk
                    ? '${ble.weatherTemp}°C · ${ble.weatherText}  (${ble.weatherCity})'
                    : 'Chưa có dữ liệu. Kết nối đồng hồ rồi bấm Cập nhật.',
                style: TextStyle(color: Colors.grey[400]),
              ),
              const SizedBox(height: 12),
              Row(
                children: [
                  Expanded(
                    child: TextField(
                      controller: _c,
                      decoration: const InputDecoration(
                        labelText: 'Thành phố',
                        isDense: true,
                        border: OutlineInputBorder(),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  FilledButton(
                    onPressed: () => ble.setWeatherCity(_c.text),
                    child: const Text('Cập nhật'),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }
}

