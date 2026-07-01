import 'dart:convert';
import 'package:http/http.dart' as http;

// ============================================================
//  Lay thoi tiet tu Open-Meteo (MIEN PHI, KHONG can API key).
//  1) Ten thanh pho -> toa do (geocoding)
//  2) Toa do -> thoi tiet hien tai (nhiet do + ma thoi tiet)
// ============================================================
class WeatherResult {
  final int temp;      // do C
  final String text;   // mo ta ngan tieng Viet
  WeatherResult(this.temp, this.text);
}

class WeatherService {
  // Ma thoi tiet WMO -> chu ngan (co dau; se duoc bo dau khi gui xuong dong ho)
  static String _codeToVi(int c) {
    if (c == 0) return 'Nắng';
    if (c <= 2) return 'Có mây';
    if (c == 3) return 'Nhiều mây';
    if (c == 45 || c == 48) return 'Sương mù';
    if (c >= 51 && c <= 57) return 'Mưa phùn';
    if (c >= 61 && c <= 67) return 'Mưa';
    if (c >= 71 && c <= 77) return 'Tuyết';
    if (c >= 80 && c <= 82) return 'Mưa rào';
    if (c >= 85 && c <= 86) return 'Mưa tuyết';
    if (c >= 95) return 'Giông';
    return 'Trời';
  }

  static Future<WeatherResult?> fetch(String city) async {
    try {
      // 1) Ten thanh pho -> lat/lon
      final geo = await http
          .get(Uri.parse(
              'https://geocoding-api.open-meteo.com/v1/search?name=${Uri.encodeComponent(city)}&count=1&language=vi&format=json'))
          .timeout(const Duration(seconds: 10));
      final gj = jsonDecode(geo.body) as Map<String, dynamic>;
      final results = gj['results'] as List?;
      if (results == null || results.isEmpty) return null;
      final lat = results[0]['latitude'];
      final lon = results[0]['longitude'];

      // 2) Thoi tiet hien tai
      final w = await http
          .get(Uri.parse(
              'https://api.open-meteo.com/v1/forecast?latitude=$lat&longitude=$lon&current=temperature_2m,weather_code&timezone=auto'))
          .timeout(const Duration(seconds: 10));
      final wj = jsonDecode(w.body) as Map<String, dynamic>;
      final cur = wj['current'] as Map<String, dynamic>?;
      if (cur == null) return null;
      final temp = (cur['temperature_2m'] as num).round();
      final code = (cur['weather_code'] as num).toInt();
      return WeatherResult(temp, _codeToVi(code));
    } catch (_) {
      return null;
    }
  }
}
