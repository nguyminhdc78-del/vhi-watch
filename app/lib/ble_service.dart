import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';

// ============================================================
//  Quan ly ket noi BLE toi dong ho VHI-Watch.
//  Singleton + ChangeNotifier de UI lang nghe cap nhat.
// ============================================================
class BleService extends ChangeNotifier {
  BleService._();
  static final BleService I = BleService._();

  // UUID khop firmware config.h
  static const _svc = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static const _navUuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  static const _timeUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  static const _statUuid = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
  static const _wpUuid    = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
  static const _routeUuid = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";

  BluetoothDevice? _device;
  BluetoothCharacteristic? _nav, _time, _stat, _wp, _route;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<List<int>>? _statSub;

  // trang thai cho UI
  bool connected = false;
  bool busy = false;
  String status = 'Chưa kết nối';
  String? deviceName;
  int battery = 0;
  bool navActive = false;
  DateTime? lastSync;

  Future<bool> _ensurePerms() async {
    final res = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();
    return res.values.every((s) => s.isGranted || s.isLimited);
  }

  Future<void> connect() async {
    if (busy || connected) return;
    busy = true;
    status = 'Đang xin quyền...';
    notifyListeners();

    if (!await _ensurePerms()) {
      busy = false;
      status = 'Thiếu quyền Bluetooth/Vị trí';
      notifyListeners();
      return;
    }

    // Bat Bluetooth neu dang tat (Android)
    try {
      if (await FlutterBluePlus.adapterState.first !=
          BluetoothAdapterState.on) {
        await FlutterBluePlus.turnOn();
      }
    } catch (_) {}

    status = 'Đang tìm VHI-Watch...';
    notifyListeners();

    // Quet tim thiet bi co service cua minh
    final found = Completer<BluetoothDevice?>();
    _scanSub = FlutterBluePlus.onScanResults.listen((results) {
      for (final r in results) {
        // luc quet, ten nam o advName; platformName thuong rong cho toi khi ket noi
        final n = r.advertisementData.advName.isNotEmpty
            ? r.advertisementData.advName
            : r.device.platformName;
        if (n.contains('VHI') && !found.isCompleted) {
          found.complete(r.device);
        }
      }
    });
    try {
      await FlutterBluePlus.startScan(
        withServices: [Guid(_svc)],
        timeout: const Duration(seconds: 10),
      );
    } catch (_) {}

    BluetoothDevice? dev;
    try {
      dev = await found.future.timeout(const Duration(seconds: 11));
    } catch (_) {
      dev = null;
    }
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    _scanSub = null;

    if (dev == null) {
      busy = false;
      status = 'Không tìm thấy đồng hồ. Kiểm tra đồng hồ đã bật?';
      notifyListeners();
      return;
    }

    status = 'Đang kết nối...';
    notifyListeners();
    _device = dev;
    _connSub = dev.connectionState.listen((s) {
      if (s == BluetoothConnectionState.disconnected) _onDisconnect();
    });

    try {
      // License.nonprofit = dung ca nhan/phi loi nhuan (mien phi) theo dieu khoan flutter_blue_plus 2.x
      await dev.connect(license: License.nonprofit, timeout: const Duration(seconds: 12));
      try {
        await dev.requestMtu(247); // MTU lon -> gui anh nen qua BLE nhanh hon
      } catch (_) {}

      final services = await dev.discoverServices();
      for (final s in services) {
        if (s.uuid.toString().toLowerCase() == _svc) {
          for (final c in s.characteristics) {
            final u = c.uuid.toString().toLowerCase();
            if (u == _navUuid) _nav = c;
            else if (u == _timeUuid) _time = c;
            else if (u == _statUuid) _stat = c;
            else if (u == _wpUuid) _wp = c;
            else if (u == _routeUuid) _route = c;
          }
        }
      }

      if (_stat != null) {
        await _stat!.setNotifyValue(true);
        _statSub = _stat!.onValueReceived.listen(_onStatus);
      }

      connected = true;
      busy = false;
      deviceName = dev.platformName;
      status = 'Đã kết nối: ${dev.platformName}';
      notifyListeners();
      await syncTime();
    } catch (e) {
      busy = false;
      status = 'Lỗi kết nối: $e';
      notifyListeners();
    }
  }

  void _onStatus(List<int> data) {
    try {
      final j = jsonDecode(utf8.decode(data)) as Map<String, dynamic>;
      battery = (j['batt'] as num?)?.toInt() ?? battery;
      navActive = ((j['nav'] as num?)?.toInt() ?? 0) == 1;
      notifyListeners();
    } catch (_) {}
  }

  void _onDisconnect() {
    connected = false;
    _nav = _time = _stat = _wp = _route = null;
    status = 'Mất kết nối';
    notifyListeners();
  }

  bool get canSendWallpaper => _wp != null;
  bool get canSendRoute => _route != null;

  // Gui duong line lo trinh (byte[0]=so diem, sau do la cac cap x,y int8)
  Future<void> sendRoute(Uint8List bytes) async {
    if (_route == null) return;
    try {
      await _route!.write(bytes, withoutResponse: true);
    } catch (_) {}
  }

  // Gui anh nen RGB565 (240x240 = 115200 byte) xuong dong ho theo tung chunk
  Future<void> sendWallpaper(Uint8List data,
      {required void Function(double) onProgress}) async {
    if (_wp == null) throw Exception('Chưa kết nối đồng hồ');
    int mtu = 23;
    try { mtu = _device?.mtuNow ?? 23; } catch (_) {}
    final int chunk = (mtu - 5).clamp(20, 240);
    for (int i = 0; i < data.length; i += chunk) {
      final int end = (i + chunk < data.length) ? i + chunk : data.length;
      await _wp!.write(data.sublist(i, end), withoutResponse: false);
      onProgress(end / data.length);
    }
  }

  Future<void> disconnect() async {
    try {
      await _statSub?.cancel();
      await _connSub?.cancel();
      await _device?.disconnect();
    } catch (_) {}
    _onDisconnect();
  }

  Future<void> syncTime() async {
    if (_time == null) return;
    final epoch = (DateTime.now().millisecondsSinceEpoch ~/ 1000).toString();
    await _time!.write(utf8.encode(epoch));
    lastSync = DateTime.now();
    notifyListeners();
  }

  Future<void> sendNav(Map<String, dynamic> obj) async {
    if (_nav == null) return;
    await _nav!.write(utf8.encode(jsonEncode(obj)));
  }

  Future<void> stopNav() => sendNav({'stop': 1});
}
