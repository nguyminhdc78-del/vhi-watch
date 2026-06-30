import 'package:flutter/material.dart';
import 'package:camera/camera.dart';
import 'package:gal/gal.dart';

// ============================================================
//  Khung chup hinh dieu khien tu dong ho (qua lenh BLE).
//  appNavKey: de mo/dong man hinh tu BleService (khong co context).
//  cameraShootSignal: tang len moi lan dong ho ra lenh "shoot".
// ============================================================
final GlobalKey<NavigatorState> appNavKey = GlobalKey<NavigatorState>();
final ValueNotifier<int> cameraShootSignal = ValueNotifier<int>(0);
bool _cameraOpen = false;

void openCameraScreen() {
  if (_cameraOpen) return;
  final nav = appNavKey.currentState;
  if (nav == null) return;
  _cameraOpen = true;
  nav.push(MaterialPageRoute(builder: (_) => const CameraScreen()))
      .then((_) => _cameraOpen = false);
}

void closeCameraScreen() {
  if (_cameraOpen) appNavKey.currentState?.maybePop();
}

void triggerShoot() => cameraShootSignal.value++;

class CameraScreen extends StatefulWidget {
  const CameraScreen({super.key});
  @override
  State<CameraScreen> createState() => _CameraScreenState();
}

class _CameraScreenState extends State<CameraScreen> {
  CameraController? _ctrl;
  bool _ready = false;
  String _status = 'Đang mở camera...';

  @override
  void initState() {
    super.initState();
    cameraShootSignal.addListener(_onShoot);
    _init();
  }

  Future<void> _init() async {
    try {
      final cams = await availableCameras();
      if (cams.isEmpty) {
        setState(() => _status = 'Không tìm thấy camera');
        return;
      }
      final back = cams.firstWhere(
          (c) => c.lensDirection == CameraLensDirection.back,
          orElse: () => cams.first);
      _ctrl = CameraController(back, ResolutionPreset.high, enableAudio: false);
      await _ctrl!.initialize();
      if (mounted) setState(() { _ready = true; _status = 'Bấm A/B trên đồng hồ để chụp'; });
    } catch (e) {
      if (mounted) setState(() => _status = 'Lỗi camera: $e');
    }
  }

  Future<void> _onShoot() async {
    if (_ctrl == null || !_ready || _ctrl!.value.isTakingPicture) return;
    try {
      final file = await _ctrl!.takePicture();
      if (!await Gal.hasAccess()) await Gal.requestAccess();
      await Gal.putImage(file.path);
      if (mounted) setState(() => _status = '✅ Đã chụp & lưu vào thư viện!');
    } catch (e) {
      if (mounted) setState(() => _status = 'Lỗi chụp: $e');
    }
  }

  @override
  void dispose() {
    cameraShootSignal.removeListener(_onShoot);
    _ctrl?.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('Chụp từ đồng hồ'),
        backgroundColor: Colors.black,
      ),
      body: Column(
        children: [
          Expanded(
            child: (_ready && _ctrl != null)
                ? CameraPreview(_ctrl!)
                : Center(
                    child: Text(_status,
                        style: const TextStyle(color: Colors.white))),
          ),
          Padding(
            padding: const EdgeInsets.all(12),
            child: Text(_status,
                style: const TextStyle(color: Colors.white70),
                textAlign: TextAlign.center),
          ),
          Padding(
            padding: const EdgeInsets.only(bottom: 24),
            child: FilledButton.icon(
              onPressed: _onShoot,
              icon: const Icon(Icons.camera),
              label: const Text('Chụp'),
            ),
          ),
        ],
      ),
    );
  }
}
