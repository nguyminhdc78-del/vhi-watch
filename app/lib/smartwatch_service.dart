import 'package:flutter/services.dart';
import 'ble_service.dart';

// ============================================================
//  Dinh tuyen thong bao (nhan tu native CallListener qua kenh vhi/media)
//  -> app nhac thi gui sang man Nhac, con lai gui thong bao thuong.
// ============================================================
class SmartwatchService {
  SmartwatchService._();
  static final SmartwatchService I = SmartwatchService._();

  static const _ch = MethodChannel('vhi/media');

  // Tu khoa nhan dien app nghe nhac (theo package name).
  // LUU Y: KHONG dung 'zing' (trung com.zing.ZALO!) va 'youtube' (trung youtube video).
  static const _musicApps = {
    'spotify', 'youtube.music', 'zing.mp3', 'zingmp3', 'nhaccuatui',
    'soundcloud', 'tidal', 'apple.android.music', 'deezer',
    'com.sec.android.app.music', 'samsung.android.app.music',
  };

  void start() {} // thong bao gio den qua BleService (native), khong con dung plugin

  // Mo man Cai dat quyen doc thong bao (native)
  Future<void> requestAccess() async {
    try { await _ch.invokeMethod('openNotifAccess'); } catch (_) {}
  }

  // Chi cac app duoc phep hien thong bao len dong ho (theo yeu cau: Zalo + Messenger)
  static const _allowedApps = {'zalo', 'orca', 'messenger'};

  // Goi tu BleService khi native day thong bao len
  void handleNotification(String pkg, String title, String text, bool removed) {
    if (!BleService.I.connected) return;
    final p = pkg.toLowerCase();
    if (_musicApps.any((m) => p.contains(m))) {   // nhac -> man Nhac (khong tinh la thong bao)
      BleService.I.sendMusic(title, text, !removed);
      return;
    }
    if (removed) return;
    if (!_allowedApps.any((m) => p.contains(m))) return;   // LOC: chi Zalo + Mess
    if (title.isEmpty && text.isEmpty) return;
    BleService.I.sendNotify(_appName(p), title, text);
  }

  // Rut gon package name -> ten app de doc
  String _appName(String pkg) {
    if (pkg.contains('messenger')) return 'Messenger';
    if (pkg.contains('zalo')) return 'Zalo';
    if (pkg.contains('whatsapp')) return 'WhatsApp';
    if (pkg.contains('telegram')) return 'Telegram';
    if (pkg.contains('gmail') || (pkg.contains('android.gm'))) return 'Gmail';
    if (pkg.contains('mms') || pkg.contains('messaging') || pkg.contains('sms')) return 'Tin nhắn';
    if (pkg.contains('dialer') || pkg.contains('incallui') || pkg.contains('phone')) return 'Cuộc gọi';
    if (pkg.contains('facebook')) return 'Facebook';
    if (pkg.contains('instagram')) return 'Instagram';
    if (pkg.contains('tiktok')) return 'TikTok';
    final parts = pkg.split('.');
    return parts.isNotEmpty ? parts.last : pkg;
  }
}
