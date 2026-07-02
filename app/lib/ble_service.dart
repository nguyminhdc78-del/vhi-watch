import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'weather_service.dart';
import 'smartwatch_service.dart';

// Bo dau tieng Viet (font dong ho khong co dau)
String vnNoAccent(String s) {
  const groups = {
    'a': 'àáạảãâầấậẩẫăằắặẳẵ', 'e': 'èéẹẻẽêềếệểễ', 'i': 'ìíịỉĩ',
    'o': 'òóọỏõôồốộổỗơờớợởỡ', 'u': 'ùúụủũưừứựửữ', 'y': 'ỳýỵỷỹ', 'd': 'đ',
  };
  var out = s;
  groups.forEach((base, accents) {
    for (final ch in accents.split('')) {
      out = out.replaceAll(ch, base).replaceAll(ch.toUpperCase(), base.toUpperCase());
    }
  });
  out = out.replaceAll('Đ', 'D').replaceAll('đ', 'd');
  // Bo moi ky tu ngoai ASCII con sot lai: dau ket hop (chu dang NFD), emoji, ky hieu la
  // -> font dong ho chi co ASCII nen day la cach chac chan het bi o vuong.
  out = out.replaceAll(RegExp(r'[^\x00-\x7F]'), '');
  return out;
}

// ============================================================
//  Quan ly ket noi BLE toi dong ho VHI-Watch.
//  Singleton + ChangeNotifier de UI lang nghe cap nhat.
// ============================================================
class BleService extends ChangeNotifier {
  BleService._() {
    // Nhan su kien cuoc goi tu native (CallListener) -> gui xuong dong ho
    _mediaCh.setMethodCallHandler((call) async {
      if (call.method == 'incomingCall') {
        if (_callShowing) return null;   // da co cuoc goi dang hien -> bo trung (SIM + thong bao)
        _callShowing = true;
        final a = call.arguments as Map?;
        sendCall(a?['name']?.toString() ?? 'Cuoc goi den', a?['app']?.toString() ?? '', true);
      } else if (call.method == 'callEnded') {
        _callShowing = false;
        sendCall('', '', false);
      } else if (call.method == 'notification') {
        final a = call.arguments as Map?;
        SmartwatchService.I.handleNotification(
          a?['pkg']?.toString() ?? '',
          a?['title']?.toString() ?? '',
          a?['text']?.toString() ?? '',
          a?['removed'] == true);
      }
      return null;
    });
  }
  static final BleService I = BleService._();

  // UUID khop firmware config.h
  static const _svc = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
  static const _navUuid = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
  static const _timeUuid = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";
  static const _statUuid = "6e400004-b5a3-f393-e0a9-e50e24dcca9e";
  static const _wpUuid     = "6e400005-b5a3-f393-e0a9-e50e24dcca9e";
  static const _routeUuid  = "6e400006-b5a3-f393-e0a9-e50e24dcca9e";
  static const _notifyUuid = "6e400007-b5a3-f393-e0a9-e50e24dcca9e";
  static const _musicUuid  = "6e400008-b5a3-f393-e0a9-e50e24dcca9e";
  static const _mediaUuid  = "6e400009-b5a3-f393-e0a9-e50e24dcca9e";
  static const _colorUuid  = "6e40000a-b5a3-f393-e0a9-e50e24dcca9e";
  static const _imgselUuid = "6e40000b-b5a3-f393-e0a9-e50e24dcca9e";
  static const _weatherUuid = "6e40000c-b5a3-f393-e0a9-e50e24dcca9e";
  static const _callUuid    = "6e40000d-b5a3-f393-e0a9-e50e24dcca9e";
  static const _contactUuid = "6e40000e-b5a3-f393-e0a9-e50e24dcca9e";
  static const _wfcfgUuid   = "6e40000f-b5a3-f393-e0a9-e50e24dcca9e";

  static const _mediaCh = MethodChannel('vhi/media'); // gui phim media Android

  BluetoothDevice? _device;
  BluetoothCharacteristic? _nav, _time, _stat, _wp, _route, _notify, _music, _media, _color, _imgsel, _weather, _call, _contact, _wfcfg;

  // Giao dien gio: vi tri (0=tren,1=giua,2=duoi) + co (3..6)
  int wfPos = 1;
  int wfSize = 4;

  // Danh ba nhanh (goi tu dong ho). Moi item: {'name':.., 'number':..}
  List<Map<String, String>> favorites = [];
  static const int maxFavorites = 12;
  bool _callShowing = false;   // dang hien 1 cuoc goi den (chong trung SIM + thong bao)
  StreamSubscription<BluetoothConnectionState>? _connSub;
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<List<int>>? _statSub;
  StreamSubscription<List<int>>? _mediaSub;
  Timer? _weatherTimer;

  // Thoi tiet (hien tren UI + gui xuong dong ho)
  String weatherCity = 'Ho Chi Minh';
  String weatherText = '';
  int weatherTemp = 0;
  bool weatherOk = false;

  // trang thai cho UI
  bool connected = false;
  bool busy = false;
  String status = 'Chưa kết nối';
  String? deviceName;
  int battery = 0;
  bool navActive = false;
  DateTime? lastSync;

  bool autoReconnect = true;   // tu dong ket noi lai khi dong ho bat nguon / vao tam song
  Timer? _reconnectTimer;

  Future<bool> _ensurePerms() async {
    final res = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
      Permission.notification,   // Android 13+: de hien thong bao dich vu nen
      Permission.phone,          // ANSWER_PHONE_CALLS: nghe/tu choi cuoc goi SIM tu dong ho
    ].request();
    // Chi bat buoc quyen Bluetooth; quyen thong bao khong co van chay duoc
    return (res[Permission.bluetoothScan]?.isGranted ?? false) &&
           (res[Permission.bluetoothConnect]?.isGranted ?? false);
  }

  // Goi khi mo app: bat dich vu nen ben bi + tu dong tim & ket noi dong ho,
  // va ket noi lai moi khi mat song (giu ket noi ca khi app chay nen).
  void autoConnectStart() {
    autoReconnect = true;
    loadFavorites();   // nap danh ba nhanh da luu
    loadWfCfg();       // nap cau hinh giao dien gio
    connect();         // startService goi SAU khi ket noi (da co quyen BLE) de tranh crash FGS
  }

  // Hen gio thu ket noi lai (khi dong ho tat/ngoai tam song se thu lai lien tuc)
  void _scheduleReconnect() {
    _reconnectTimer?.cancel();
    if (!autoReconnect) return;
    _reconnectTimer = Timer(const Duration(seconds: 4), () {
      if (autoReconnect && !connected && !busy) connect();
    });
  }

  Future<void> connect() async {
    if (busy || connected) return;
    autoReconnect = true;   // bam ket noi -> bat lai tu dong ket noi lai
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
    try { await FlutterBluePlus.stopScan(); } catch (_) {}   // dung scan cu con ket (neu co)
    try {
      // Quet TAT CA thiet bi, che do do tre thap (nhay hon), loc theo ten "VHI"
      await FlutterBluePlus.startScan(
        timeout: const Duration(seconds: 10),
        androidScanMode: AndroidScanMode.lowLatency,
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
      status = 'Đang chờ đồng hồ bật...';
      notifyListeners();
      _scheduleReconnect();   // dong ho chua bat -> thu lai sau
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

      final services = await dev.discoverServices().timeout(const Duration(seconds: 15));
      for (final s in services) {
        if (s.uuid.toString().toLowerCase() == _svc) {
          for (final c in s.characteristics) {
            final u = c.uuid.toString().toLowerCase();
            if (u == _navUuid) _nav = c;
            else if (u == _timeUuid) _time = c;
            else if (u == _statUuid) _stat = c;
            else if (u == _wpUuid) _wp = c;
            else if (u == _routeUuid) _route = c;
            else if (u == _notifyUuid) _notify = c;
            else if (u == _musicUuid) _music = c;
            else if (u == _mediaUuid) _media = c;
            else if (u == _colorUuid) _color = c;
            else if (u == _imgselUuid) _imgsel = c;
            else if (u == _weatherUuid) _weather = c;
            else if (u == _callUuid) _call = c;
            else if (u == _contactUuid) _contact = c;
            else if (u == _wfcfgUuid) _wfcfg = c;
          }
        }
      }

      if (_stat != null) {
        await _stat!.setNotifyValue(true);
        _statSub = _stat!.onValueReceived.listen(_onStatus);
      }

      // lang nghe lenh dieu khien nhac tu dong ho -> bam phim media Android
      if (_media != null) {
        await _media!.setNotifyValue(true);
        _mediaSub = _media!.onValueReceived.listen(_onMediaCmd);
      }

      connected = true;
      busy = false;
      deviceName = dev.platformName;
      status = 'Đã kết nối: ${dev.platformName}';
      notifyListeners();
      _mediaCh.invokeMethod('startService').catchError((_) {});  // giu ket noi nen (da co quyen BLE)
      await syncTime();
      _mediaCh.invokeMethod('watchCalls').catchError((_) {});   // theo doi cuoc goi SIM
      // Dời việc nặng (thời tiết + danh bạ) ra sau vài giây cho link ổn định truoc
      Future.delayed(const Duration(seconds: 3), () {
        if (connected) { _startWeatherLoop(); syncContacts(); sendWfCfg(); }
      });
    } catch (e) {
      busy = false;
      status = 'Lỗi kết nối: $e';
      notifyListeners();
      _scheduleReconnect();   // ket noi loi -> thu lai
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
    busy = false;                 // tranh ket co busy khi rot giua chung
    _callShowing = false;         // reset de cuoc goi sau con hien duoc
    _mediaSub?.cancel(); _mediaSub = null;
    _statSub?.cancel();  _statSub = null;
    _connSub?.cancel();  _connSub = null;   // huy listener cu -> khong chong cheo
    _weatherTimer?.cancel();
    _nav = _time = _stat = _wp = _route = _notify = _music = _media = _color = _imgsel = _weather = _call = _contact = _wfcfg = null;
    status = autoReconnect ? 'Mất kết nối - đang kết nối lại...' : 'Mất kết nối';
    notifyListeners();
    _scheduleReconnect();   // tu dong ket noi lai (vd dong ho ngu/bat lai)
  }

  // Dong ho gui "next"/"prev"/"playpause" -> bam phim media he thong
  void _onMediaCmd(List<int> data) {
    final cmd = utf8.decode(data, allowMalformed: true);
    // Lenh camera: mo APP MAY ANH GOC cua dien thoai (dong ho chup bang HID Volume)
    if (cmd == 'camera_open') {
      _mediaCh.invokeMethod('openCamera').catchError((_) {});
      return;
    }
    // Tim dien thoai: reo to + rung / tat reo
    if (cmd == 'findphone') {
      _mediaCh.invokeMethod('findPhone').catchError((_) {});
      return;
    }
    if (cmd == 'findphone_stop') {
      _mediaCh.invokeMethod('findPhoneStop').catchError((_) {});
      return;
    }
    // Cuoc goi: dong ho bam Nghe / Tu choi -> ban lai nut cua app dang goi
    if (cmd == 'call_answer') { _mediaCh.invokeMethod('callAnswer').catchError((_) {}); return; }
    if (cmd == 'call_reject') { _mediaCh.invokeMethod('callReject').catchError((_) {}); return; }
    // Goi tu dong ho: "dial:<so>" -> dien thoai tu quay so
    if (cmd.startsWith('dial:')) {
      final num = cmd.substring(5);
      _mediaCh.invokeMethod('dialNumber', {'number': num}).catchError((_) {});
      return;
    }
    // Lenh nhac -> bam phim media he thong
    int code = 0;
    if (cmd == 'next') code = 87;          // KEYCODE_MEDIA_NEXT
    else if (cmd == 'prev') code = 88;     // KEYCODE_MEDIA_PREVIOUS
    else if (cmd == 'playpause') code = 85;// KEYCODE_MEDIA_PLAY_PAUSE
    if (code != 0) {
      _mediaCh.invokeMethod('mediaKey', {'code': code}).catchError((_) {});
    }
  }

  bool get canSendRoute => _route != null;
  bool get canSetColor => _color != null;

  // Gui mau chu (R,G,B 0..255) xuong dong ho
  Future<void> sendColor(int r, int g, int b) async {
    if (_color == null) return;
    try {
      await _color!.write([r & 0xff, g & 0xff, b & 0xff], withoutResponse: true);
    } catch (_) {}
  }

  // --- Giao dien gio (vi tri + co) ---
  Future<void> loadWfCfg() async {
    try {
      final sp = await SharedPreferences.getInstance();
      wfPos = sp.getInt('wfPos') ?? 1;
      wfSize = sp.getInt('wfSize') ?? 4;
      notifyListeners();
    } catch (_) {}
  }
  Future<void> sendWfCfg() async {
    if (_wfcfg == null) return;
    try { await _wfcfg!.write([wfPos & 0xff, wfSize & 0xff], withoutResponse: true); } catch (_) {}
  }
  Future<void> setWfLayout({int? pos, int? size}) async {
    if (pos != null) wfPos = pos;
    if (size != null) wfSize = size;
    notifyListeners();
    try {
      final sp = await SharedPreferences.getInstance();
      await sp.setInt('wfPos', wfPos);
      await sp.setInt('wfSize', wfSize);
    } catch (_) {}
    await sendWfCfg();
  }

  // --- Thoi tiet ---
  void _startWeatherLoop() {
    _weatherTimer?.cancel();
    sendWeather();
    _weatherTimer = Timer.periodic(const Duration(minutes: 30), (_) => sendWeather());
  }

  // Lay thoi tiet theo thanh pho hien tai roi gui xuong dong ho
  Future<void> sendWeather() async {
    final r = await WeatherService.fetch(weatherCity);
    if (r == null) return;
    weatherTemp = r.temp;
    weatherText = r.text;
    weatherOk = true;
    notifyListeners();
    if (_weather == null) return;
    final js = jsonEncode({'t': r.temp, 'w': vnNoAccent(r.text)});
    try { await _weather!.write(utf8.encode(js), withoutResponse: true); } catch (_) {}
  }

  // Doi thanh pho -> lay lai thoi tiet ngay
  Future<void> setWeatherCity(String city) async {
    weatherCity = city.trim().isEmpty ? weatherCity : city.trim();
    notifyListeners();
    await sendWeather();
  }

  // --- Cuoc goi den: gui xuong dong ho ---
  Future<void> sendCall(String name, String app, bool ringing) async {
    if (_call == null) return;
    final js = ringing
        ? jsonEncode({'st': 1, 'name': _clipBytes(name, 88), 'app': _clipBytes(app, 28)})
        : jsonEncode({'st': 0});
    try { await _call!.write(utf8.encode(js), withoutResponse: true); } catch (_) {}
  }

  // --- Danh ba nhanh ---
  Future<void> loadFavorites() async {
    try {
      final sp = await SharedPreferences.getInstance();
      final raw = sp.getString('favorites');
      if (raw != null) {
        final list = jsonDecode(raw) as List;
        favorites = list.map((e) => {
          'name': (e['name'] ?? '').toString(),
          'number': (e['number'] ?? '').toString(),
        }).toList();
        notifyListeners();
      }
    } catch (_) {}
  }

  Future<void> _saveFavorites() async {
    try {
      final sp = await SharedPreferences.getInstance();
      await sp.setString('favorites', jsonEncode(favorites));
    } catch (_) {}
  }

  Future<void> addFavorite(String name, String number) async {
    name = name.trim();
    number = number.replaceAll(RegExp(r'[^0-9+]'), ''); // chi giu so va dau +
    if (name.isEmpty || number.isEmpty) return;
    if (favorites.length >= maxFavorites) return;
    favorites.add({'name': name, 'number': number});
    await _saveFavorites();
    notifyListeners();
    await syncContacts();
  }

  Future<void> removeFavorite(int i) async {
    if (i < 0 || i >= favorites.length) return;
    favorites.removeAt(i);
    await _saveFavorites();
    notifyListeners();
    await syncContacts();
  }

  // Gui toan bo danh ba nhanh xuong dong ho ("C" xoa het, roi tung "ten\tso")
  Future<void> syncContacts() async {
    if (_contact == null) return;
    try {
      await _contact!.write(utf8.encode('C'), withoutResponse: true);
      await Future.delayed(const Duration(milliseconds: 40));
      for (final f in favorites) {
        final line = '${_clipBytes(f['name'] ?? '', 26)}\t${f['number']}';
        await _contact!.write(utf8.encode(line), withoutResponse: true);
        await Future.delayed(const Duration(milliseconds: 40));
      }
    } catch (_) {}
  }

  // Kiem tra quyen goi truc tiep (CALL_PHONE) o tang he thong (chinh xac)
  Future<bool> hasCallPermission() async {
    try {
      final r = await _mediaCh.invokeMethod('hasCallPerm');
      return r == true;
    } catch (_) { return false; }
  }

  // Quyen "Hien thi tren app khac" (de bat cuoc goi khi app o nen)
  Future<bool> hasOverlay() async {
    try {
      final r = await _mediaCh.invokeMethod('hasOverlay');
      return r == true;
    } catch (_) { return false; }
  }
  Future<void> openOverlay() async {
    try { await _mediaCh.invokeMethod('openOverlay'); } catch (_) {}
  }

  // Chon 1 lien he tu danh ba dien thoai (native contact picker)
  Future<void> pickFromContacts() async {
    try {
      final r = await _mediaCh.invokeMethod('pickContact');
      if (r is Map) {
        await addFavorite(r['name']?.toString() ?? '', r['number']?.toString() ?? '');
      }
    } catch (_) {}
  }


  String _clip(String s, int n) => s.length > n ? s.substring(0, n) : s;

  // Cat chuoi theo so BYTE UTF-8 (khong cat giua 1 ky tu) de vua goi BLE
  String _clipBytes(String s, int maxBytes) {
    if (utf8.encode(s).length <= maxBytes) return s;
    final buf = StringBuffer();
    int used = 0;
    for (final r in s.runes) {
      final cb = utf8.encode(String.fromCharCode(r)).length;
      if (used + cb > maxBytes) break;
      buf.writeCharCode(r);
      used += cb;
    }
    return buf.toString();
  }

  // Gui thong bao xuong dong ho
  Future<void> sendNotify(String app, String title, String text) async {
    if (_notify == null) return;
    final js = jsonEncode({
      'app': _clipBytes(app, 22),
      'title': _clipBytes(title, 80),
      'text': _clipBytes(text, 96),
    });
    try { await _notify!.write(utf8.encode(js), withoutResponse: true); } catch (_) {}
  }

  // Gui bai hat dang phat xuong dong ho
  Future<void> sendMusic(String title, String artist, bool playing) async {
    if (_music == null) return;
    final js = jsonEncode({
      'title': _clipBytes(title, 88),
      'artist': _clipBytes(artist, 88),
      'playing': playing ? 1 : 0,
    });
    try { await _music!.write(utf8.encode(js), withoutResponse: true); } catch (_) {}
  }

  // Gui duong line lo trinh (byte[0]=so diem, sau do la cac cap x,y int8)
  Future<void> sendRoute(Uint8List bytes) async {
    if (_route == null) return;
    try {
      await _route!.write(bytes, withoutResponse: true);
    } catch (_) {}
  }

  bool get canUploadImage => _wp != null && _imgsel != null;

  // Gui anh RGB565 (240x240 = 115200 byte) xuong dong ho.
  // target: 0xFF = anh nen; 0..3 = o QR.
  Future<void> sendImage(int target, Uint8List data,
      {required void Function(double) onProgress}) async {
    if (_wp == null || _imgsel == null) throw Exception('Chưa kết nối đồng hồ');
    await _imgsel!.write([target & 0xff], withoutResponse: true);   // chon dich
    await Future.delayed(const Duration(milliseconds: 60));
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
    autoReconnect = false;          // nguoi dung chu dong ngat -> khong tu ket noi lai
    _reconnectTimer?.cancel();
    _mediaCh.invokeMethod('stopService').catchError((_) {});   // tat dich vu nen
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
    try {
      await _time!.write(utf8.encode(epoch), withoutResponse: true);
      lastSync = DateTime.now();
      notifyListeners();
    } catch (_) {}
  }

  Future<void> sendNav(Map<String, dynamic> obj) async {
    if (_nav == null) return;
    await _nav!.write(utf8.encode(jsonEncode(obj)));
  }

  Future<void> stopNav() => sendNav({'stop': 1});
}
