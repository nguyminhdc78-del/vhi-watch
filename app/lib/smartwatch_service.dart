import 'dart:async';
import 'package:notification_listener_service/notification_listener_service.dart';
import 'package:notification_listener_service/notification_event.dart';
import 'ble_service.dart';

// ============================================================
//  Doc thong bao Android -> chuyen tiep xuong dong ho qua BLE.
//  Phan biet app nhac (gui sang man Nhac) va thong bao thuong.
// ============================================================
class SmartwatchService {
  SmartwatchService._();
  static final SmartwatchService I = SmartwatchService._();

  StreamSubscription<ServiceNotificationEvent>? _sub;

  // Tu khoa nhan dien app nghe nhac (theo package name)
  static const _musicApps = {
    'spotify', 'youtube.music', 'youtube', 'zing', 'nhaccuatui',
    'soundcloud', 'tidal', 'apple.android.music', 'deezer', 'music',
  };

  Future<bool> isGranted() => NotificationListenerService.isPermissionGranted();
  Future<bool> requestAccess() => NotificationListenerService.requestPermission();

  void start() {
    _sub ??= NotificationListenerService.notificationsStream.listen(_onNotif);
  }

  void _onNotif(ServiceNotificationEvent e) {
    if (!BleService.I.connected) return;
    final pkg = e.packageName.toLowerCase();
    final title = e.title;
    final content = e.content;
    final removed = e.hasRemoved;

    if (_musicApps.any((m) => pkg.contains(m))) {
      BleService.I.sendMusic(title, content, !removed);
      return;
    }
    if (removed) return;
    if (title.isEmpty && content.isEmpty) return;
    BleService.I.sendNotify(_appName(pkg), title, content);
  }

  // Rut gon package name -> ten app de doc
  String _appName(String pkg) {
    if (pkg.contains('messenger')) return 'Messenger';
    if (pkg.contains('zalo')) return 'Zalo';
    if (pkg.contains('whatsapp')) return 'WhatsApp';
    if (pkg.contains('telegram')) return 'Telegram';
    if (pkg.contains('gmail') || (pkg.contains('android.gm'))) return 'Gmail';
    if (pkg.contains('mms') || pkg.contains('messaging') || pkg.contains('sms')) return 'Tin nhan';
    if (pkg.contains('dialer') || pkg.contains('incallui') || pkg.contains('phone')) return 'Cuoc goi';
    if (pkg.contains('facebook')) return 'Facebook';
    if (pkg.contains('instagram')) return 'Instagram';
    if (pkg.contains('tiktok')) return 'TikTok';
    final parts = pkg.split('.');
    return parts.isNotEmpty ? parts.last : pkg;
  }
}
