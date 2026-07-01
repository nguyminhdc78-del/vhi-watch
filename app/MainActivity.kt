package com.vhitek.vhi_watch

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Build
import android.os.IBinder
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.MediaStore
import android.view.KeyEvent
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

// Kenh "vhi/media": Flutter goi -> gui phim media he thong / mo camera / reo tim dien thoai
class MainActivity : FlutterActivity() {
    private val channel = "vhi/media"
    private var ringtone: Ringtone? = null

    private fun vibrator(): Vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(flutterEngine.dartExecutor.binaryMessenger, channel).setMethodCallHandler { call, result ->
            when (call.method) {
                "mediaKey" -> {
                    val code = call.argument<Int>("code") ?: 0
                    val am = getSystemService(Context.AUDIO_SERVICE) as AudioManager
                    am.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_DOWN, code))
                    am.dispatchMediaKeyEvent(KeyEvent(KeyEvent.ACTION_UP, code))
                    result.success(true)
                }
                "openCamera" -> {
                    try {
                        // Mo app may anh GOC cua dien thoai (khong phai trong app)
                        val intent = Intent(MediaStore.INTENT_ACTION_STILL_IMAGE_CAMERA)
                        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                        startActivity(intent)
                        result.success(true)
                    } catch (e: Exception) {
                        result.success(false)
                    }
                }
                "findPhone" -> {
                    try {
                        // Reo to + rung de tim dien thoai (dung stream ALARM -> keu ca khi de im)
                        val am = getSystemService(Context.AUDIO_SERVICE) as AudioManager
                        am.setStreamVolume(AudioManager.STREAM_ALARM,
                            am.getStreamMaxVolume(AudioManager.STREAM_ALARM), 0)
                        var uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
                            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
                        ringtone?.stop()
                        ringtone = RingtoneManager.getRingtone(applicationContext, uri)?.apply {
                            audioAttributes = AudioAttributes.Builder()
                                .setUsage(AudioAttributes.USAGE_ALARM)
                                .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                                .build()
                            if (Build.VERSION.SDK_INT >= 28) isLooping = true
                            play()
                        }
                        val pattern = longArrayOf(0, 600, 300)
                        if (Build.VERSION.SDK_INT >= 26)
                            vibrator().vibrate(VibrationEffect.createWaveform(pattern, 0))
                        else @Suppress("DEPRECATION") vibrator().vibrate(pattern, 0)
                        result.success(true)
                    } catch (e: Exception) {
                        result.success(false)
                    }
                }
                "findPhoneStop" -> {
                    try {
                        ringtone?.stop()
                        ringtone = null
                        vibrator().cancel()
                    } catch (_: Exception) {}
                    result.success(true)
                }
                "startService" -> {
                    val i = Intent(this, WatchService::class.java)
                    if (Build.VERSION.SDK_INT >= 26) startForegroundService(i) else startService(i)
                    result.success(true)
                }
                "stopService" -> {
                    stopService(Intent(this, WatchService::class.java))
                    result.success(true)
                }
                else -> result.notImplemented()
            }
        }
    }
}

// ============================================================
//  Dich vu nen ben bi: giu ket noi Bluetooth dong ho khi app chay nen
//  (giong cach smartwatch that lam - co 1 thong bao thuong tru nho, im lang)
// ============================================================
class WatchService : Service() {
    companion object {
        const val CHANNEL_ID = "vhi_watch_bg"
        const val NOTI_ID = 1001
    }

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTI_ID, buildNotification())
        return START_STICKY   // he thong giet -> tu bat lai
    }

    private fun buildNotification(): Notification {
        if (Build.VERSION.SDK_INT >= 26) {
            val ch = NotificationChannel(CHANNEL_ID, "VHI Watch nen",
                NotificationManager.IMPORTANCE_MIN)   // do uu tien thap nhat -> im lang, gap gon
            ch.setShowBadge(false)
            (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager)
                .createNotificationChannel(ch)
        }
        @Suppress("DEPRECATION")
        val builder = if (Build.VERSION.SDK_INT >= 26)
            Notification.Builder(this, CHANNEL_ID) else Notification.Builder(this)
        return builder
            .setContentTitle("VHI Watch")
            .setContentText("Dang giu ket noi dong ho")
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setOngoing(true)
            .build()
    }
}
