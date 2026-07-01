import 'package:flutter/material.dart';
import 'theme.dart';
import 'ble_service.dart';
import 'smartwatch_service.dart';
import 'screens/home_screen.dart';
import 'screens/wallpaper_screen.dart';
import 'screens/quick_call_screen.dart';

void main() => runApp(const VhiApp());

class VhiApp extends StatelessWidget {
  const VhiApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'VHI Watch',
      debugShowCheckedModeBanner: false,
      theme: buildTheme(),
      home: const RootPage(),
    );
  }
}

class RootPage extends StatefulWidget {
  const RootPage({super.key});
  @override
  State<RootPage> createState() => _RootPageState();
}

class _RootPageState extends State<RootPage> {
  int _idx = 0;
  final _pages = const [HomeScreen(), QuickCallScreen(), WallpaperScreen()];

  @override
  void initState() {
    super.initState();
    SmartwatchService.I.start();      // bat dau lang nghe thong bao
    BleService.I.autoConnectStart();  // tu dong ket noi dong ho khi mo app
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('⌚ VHI Watch'),
        actions: [
          ListenableBuilder(
            listenable: BleService.I,
            builder: (_, __) {
              final on = BleService.I.connected;
              return Padding(
                padding: const EdgeInsets.only(right: 12),
                child: Chip(
                  avatar: CircleAvatar(
                    radius: 5,
                    backgroundColor: on ? AppColors.ok : AppColors.danger,
                  ),
                  label: Text(on ? 'Đã kết nối' : 'Chưa nối',
                      style: const TextStyle(fontSize: 12)),
                  visualDensity: VisualDensity.compact,
                ),
              );
            },
          ),
        ],
      ),
      body: IndexedStack(index: _idx, children: _pages),
      bottomNavigationBar: NavigationBar(
        selectedIndex: _idx,
        onDestinationSelected: (i) => setState(() => _idx = i),
        destinations: const [
          NavigationDestination(icon: Icon(Icons.watch), label: 'Trang chủ'),
          NavigationDestination(icon: Icon(Icons.call), label: 'Gọi nhanh'),
          NavigationDestination(icon: Icon(Icons.image), label: 'Ảnh nền'),
        ],
      ),
    );
  }
}
