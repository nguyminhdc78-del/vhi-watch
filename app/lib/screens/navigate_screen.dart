import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:geolocator/geolocator.dart';
import 'package:http/http.dart' as http;
import '../ble_service.dart';

class NavigateScreen extends StatefulWidget {
  const NavigateScreen({super.key});
  @override
  State<NavigateScreen> createState() => _NavigateScreenState();
}

class _NavStep {
  final double lat, lng;
  final int man;
  final String text;
  _NavStep(this.lat, this.lng, this.man, this.text);
}

class _NavigateScreenState extends State<NavigateScreen> {
  final _apiKey = TextEditingController();
  final _dest = TextEditingController();
  final _street = TextEditingController(text: 'Rẽ phải vào Lê Lợi');
  final _dist = TextEditingController(text: '150');
  final _remain = TextEditingController(text: '3200');
  final _eta = TextEditingController(text: '12:34');
  int _man = 1;
  String _navStat = '';

  StreamSubscription<Position>? _posSub;
  List<_NavStep> _steps = [];
  int _idx = 0;

  final _ble = BleService.I;

  @override
  void dispose() {
    _posSub?.cancel();
    super.dispose();
  }

  // Doi ma huong re cua OpenRouteService (so) -> enum firmware
  int _mapMan(int t) {
    switch (t) {
      case 0: return 2;   // turn left
      case 1: return 3;   // turn right
      case 2: return 7;   // sharp left
      case 3: return 6;   // sharp right
      case 4: return 4;   // slight left
      case 5: return 5;   // slight right
      case 9: return 8;   // u-turn
      case 10: return 9;  // goal/arrive
      case 12: return 4;  // keep left
      case 13: return 5;  // keep right
      default: return 1;  // straight / depart / roundabout
    }
  }

  Future<void> _startRoute() async {
    if (!_ble.connected) {
      setState(() => _navStat = 'Hãy kết nối đồng hồ trước');
      return;
    }
    final key = _apiKey.text.trim();
    final dest = _dest.text.trim();
    if (key.isEmpty || dest.isEmpty) {
      setState(() => _navStat = 'Nhập API key và điểm đến');
      return;
    }
    setState(() => _navStat = 'Đang lấy vị trí...');

    var perm = await Geolocator.checkPermission();
    if (perm == LocationPermission.denied) {
      perm = await Geolocator.requestPermission();
    }
    if (perm == LocationPermission.denied ||
        perm == LocationPermission.deniedForever) {
      setState(() => _navStat = 'Bị từ chối quyền vị trí');
      return;
    }
    final pos = await Geolocator.getCurrentPosition();

    try {
      // 1) Geocode: doi ten dia diem -> toa do (lon,lat)
      setState(() => _navStat = 'Đang tìm điểm đến...');
      final geoUrl = Uri.parse(
          'https://api.openrouteservice.org/geocode/search'
          '?api_key=$key&text=${Uri.encodeComponent(dest)}&size=1');
      final geoRes = await http.get(geoUrl);
      final geo = jsonDecode(geoRes.body) as Map<String, dynamic>;
      final feats = geo['features'] as List?;
      if (feats == null || feats.isEmpty) {
        setState(() => _navStat = 'Không tìm thấy điểm đến (kiểm tra key/điểm đến)');
        return;
      }
      final dc = feats[0]['geometry']['coordinates'] as List; // [lon, lat]
      final double dLon = (dc[0] as num).toDouble();
      final double dLat = (dc[1] as num).toDouble();

      // 2) Tinh tuyen duong
      setState(() => _navStat = 'Đang tính đường...');
      final dirUrl = Uri.parse(
          'https://api.openrouteservice.org/v2/directions/driving-car'
          '?api_key=$key&start=${pos.longitude},${pos.latitude}&end=$dLon,$dLat');
      final dirRes = await http.get(dirUrl);
      final dir = jsonDecode(dirRes.body) as Map<String, dynamic>;
      final dfeats = dir['features'] as List?;
      if (dfeats == null || dfeats.isEmpty) {
        setState(() => _navStat = 'Lỗi định tuyến (xem lại key)');
        return;
      }
      final coords = dfeats[0]['geometry']['coordinates'] as List; // [[lon,lat],...]
      final segments = dfeats[0]['properties']['segments'] as List;
      _steps = [];
      for (final seg in segments) {
        for (final s in (seg['steps'] as List)) {
          final wp = s['way_points'] as List; // [startIdx, endIdx]
          int endIdx = (wp[1] as num).toInt();
          if (endIdx < 0) endIdx = 0;
          if (endIdx >= coords.length) endIdx = coords.length - 1;
          final c = coords[endIdx] as List; // [lon, lat]
          _steps.add(_NavStep(
            (c[1] as num).toDouble(), // lat
            (c[0] as num).toDouble(), // lng
            _mapMan((s['type'] as num).toInt()),
            (s['instruction'] ?? '').toString(),
          ));
        }
      }
      _idx = 0;
      final dist = (dfeats[0]['properties']['summary']?['distance'] ?? 0) as num;
      setState(() => _navStat =
          'Bắt đầu: ${_steps.length} bước · ${(dist / 1000).toStringAsFixed(1)} km');

      _posSub?.cancel();
      _posSub = Geolocator.getPositionStream(
        locationSettings: const LocationSettings(
            accuracy: LocationAccuracy.high, distanceFilter: 5),
      ).listen(_onMove);
    } catch (e) {
      setState(() => _navStat = 'Lỗi: $e');
    }
  }

  void _onMove(Position pos) {
    if (_idx >= _steps.length) return;
    final st = _steps[_idx];
    final d = Geolocator.distanceBetween(
            pos.latitude, pos.longitude, st.lat, st.lng)
        .round();
    var remain = d;
    for (var i = _idx + 1; i < _steps.length; i++) {
      remain += Geolocator.distanceBetween(
              _steps[i - 1].lat, _steps[i - 1].lng, _steps[i].lat, _steps[i].lng)
          .round();
    }
    _ble.sendNav({'m': st.man, 'd': d, 'r': remain, 's': st.text, 'e': ''});
    setState(() => _navStat = 'Bước ${_idx + 1}/${_steps.length} · ${d}m · ${st.text}');
    if (d < 25) {
      _idx++;
      if (_idx >= _steps.length) {
        _ble.sendNav({'m': 9, 'd': 0, 'r': 0, 's': 'Đã đến nơi', 'e': ''});
        _posSub?.cancel();
      }
    }
  }

  void _stopRoute() {
    _posSub?.cancel();
    _ble.stopNav();
    setState(() => _navStat = 'Đã dừng');
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
                const Text('Dẫn đường tự động',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                const SizedBox(height: 4),
                Text('GPS điện thoại + OpenRouteService đẩy từng bước rẽ xuống đồng hồ. (Key miễn phí tại openrouteservice.org)',
                    style: TextStyle(color: Colors.grey[400], fontSize: 13)),
                const SizedBox(height: 12),
                TextField(
                  controller: _apiKey,
                  decoration: const InputDecoration(labelText: 'OpenRouteService API Key'),
                ),
                const SizedBox(height: 10),
                TextField(
                  controller: _dest,
                  decoration: const InputDecoration(
                      labelText: 'Điểm đến', hintText: 'VD: Hồ Hoàn Kiếm, Hà Nội'),
                ),
                const SizedBox(height: 12),
                FilledButton.icon(
                  onPressed: _startRoute,
                  icon: const Icon(Icons.play_arrow),
                  label: const Text('Bắt đầu dẫn đường'),
                ),
                const SizedBox(height: 8),
                OutlinedButton(onPressed: _stopRoute, child: const Text('Dừng')),
                if (_navStat.isNotEmpty) ...[
                  const SizedBox(height: 10),
                  Text(_navStat, style: TextStyle(color: Colors.grey[300])),
                ],
              ],
            ),
          ),
        ),
        Card(
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Text('Gửi lệnh rẽ thủ công (test)',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600)),
                const SizedBox(height: 12),
                DropdownButtonFormField<int>(
                  value: _man,
                  decoration: const InputDecoration(labelText: 'Hướng rẽ'),
                  items: const [
                    DropdownMenuItem(value: 1, child: Text('⬆️ Đi thẳng')),
                    DropdownMenuItem(value: 2, child: Text('⬅️ Rẽ trái')),
                    DropdownMenuItem(value: 3, child: Text('➡️ Rẽ phải')),
                    DropdownMenuItem(value: 4, child: Text('↖️ Chếch trái')),
                    DropdownMenuItem(value: 5, child: Text('↗️ Chếch phải')),
                    DropdownMenuItem(value: 8, child: Text('↩️ Quay đầu')),
                    DropdownMenuItem(value: 9, child: Text('🏁 Đã đến nơi')),
                  ],
                  onChanged: (v) => setState(() => _man = v ?? 1),
                ),
                const SizedBox(height: 10),
                Row(children: [
                  Expanded(
                    child: TextField(
                      controller: _dist,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(labelText: 'Cách (m)'),
                    ),
                  ),
                  const SizedBox(width: 10),
                  Expanded(
                    child: TextField(
                      controller: _remain,
                      keyboardType: TextInputType.number,
                      decoration: const InputDecoration(labelText: 'Còn lại (m)'),
                    ),
                  ),
                ]),
                const SizedBox(height: 10),
                TextField(
                  controller: _street,
                  decoration: const InputDecoration(labelText: 'Tên đường'),
                ),
                const SizedBox(height: 10),
                TextField(
                  controller: _eta,
                  decoration: const InputDecoration(labelText: 'ETA'),
                ),
                const SizedBox(height: 12),
                FilledButton.icon(
                  onPressed: () => _ble.sendNav({
                    'm': _man,
                    'd': int.tryParse(_dist.text) ?? 0,
                    'r': int.tryParse(_remain.text) ?? 0,
                    's': _street.text,
                    'e': _eta.text,
                  }),
                  icon: const Icon(Icons.send),
                  label: const Text('Gửi xuống đồng hồ'),
                ),
                const SizedBox(height: 8),
                OutlinedButton(
                    onPressed: _ble.stopNav, child: const Text('Dừng dẫn đường')),
              ],
            ),
          ),
        ),
      ],
    );
  }
}
