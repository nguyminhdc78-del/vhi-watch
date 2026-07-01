package com.vhitek.vhi_watch

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.Manifest
import android.app.Activity
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.provider.ContactsContract
import android.telecom.TelecomManager
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.provider.MediaStore
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.view.KeyEvent
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

// Kenh "vhi/media": Flutter goi -> gui phim media he thong / mo camera / reo tim dien thoai
class MainActivity : FlutterActivity() {
    private val channel = "vhi/media"
    private var ringtone: Ringtone? = null

    private fun vibrator(): Vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

    companion object {
        var mediaChannel: MethodChannel? = null   // de dich vu nen goi nguoc ve Flutter
    }

    private var pendingPick: MethodChannel.Result? = null
    private val REQ_PICK = 7001

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQ_PICK) return
        val res = pendingPick; pendingPick = null
        if (resultCode == Activity.RESULT_OK && data?.data != null) {
            try {
                contentResolver.query(data.data!!, arrayOf(
                    ContactsContract.CommonDataKinds.Phone.DISPLAY_NAME,
                    ContactsContract.CommonDataKinds.Phone.NUMBER), null, null, null)?.use {
                    if (it.moveToFirst()) {
                        res?.success(mapOf("name" to (it.getString(0) ?: ""), "number" to (it.getString(1) ?: "")))
                        return
                    }
                }
            } catch (_: Exception) {}
        }
        res?.success(null)
    }

    private fun hasAnswerPerm(): Boolean =
        checkSelfPermission(Manifest.permission.ANSWER_PHONE_CALLS) == PackageManager.PERMISSION_GRANTED

    // Nghe may: SIM dung TelecomManager (dang tin cay), VoIP ban lai nut Nghe cua thong bao
    private fun answerCall() {
        if (Build.VERSION.SDK_INT >= 26 && hasAnswerPerm()) {
            try { (getSystemService(Context.TELECOM_SERVICE) as TelecomManager).acceptRingingCall() } catch (_: Exception) {}
        }
        CallListener.answer()
    }

    private fun rejectCall() {
        if (Build.VERSION.SDK_INT >= 28 && hasAnswerPerm()) {
            try { (getSystemService(Context.TELECOM_SERVICE) as TelecomManager).endCall() } catch (_: Exception) {}
        }
        CallListener.reject()
    }

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        val ch = MethodChannel(flutterEngine.dartExecutor.binaryMessenger, channel)
        mediaChannel = ch
        ch.setMethodCallHandler { call, result ->
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
                "callAnswer" -> { answerCall(); result.success(true) }
                "callReject" -> { rejectCall(); result.success(true) }
                "hasCallPerm" -> {
                    result.success(checkSelfPermission(Manifest.permission.CALL_PHONE)
                        == PackageManager.PERMISSION_GRANTED)
                }
                "openNotifAccess" -> {
                    val i = Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)
                    i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    startActivity(i)
                    result.success(true)
                }
                "pickContact" -> {
                    pendingPick = result
                    try {
                        startActivityForResult(
                            Intent(Intent.ACTION_PICK, ContactsContract.CommonDataKinds.Phone.CONTENT_URI),
                            REQ_PICK)
                    } catch (e: Exception) { pendingPick = null; result.success(null) }
                }
                "dialNumber" -> {
                    val num = call.argument<String>("number") ?: ""
                    val uri = Uri.parse("tel:" + Uri.encode(num))
                    var ok = false
                    if (checkSelfPermission(Manifest.permission.CALL_PHONE) == PackageManager.PERMISSION_GRANTED) {
                        // 1) placeCall: goi thang qua he thong, chay ca khi app o nen
                        try {
                            (getSystemService(Context.TELECOM_SERVICE) as TelecomManager).placeCall(uri, null)
                            ok = true
                        } catch (_: Exception) {}
                        // 2) du phong: mo cuoc goi bang intent
                        if (!ok) try {
                            startActivity(Intent(Intent.ACTION_CALL, uri).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                            ok = true
                        } catch (_: Exception) {}
                    }
                    if (!ok) try {
                        startActivity(Intent(Intent.ACTION_DIAL, uri).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                        ok = true
                    } catch (_: Exception) {}
                    result.success(ok)
                }
                "hasOverlay" -> result.success(Settings.canDrawOverlays(this))
                "openOverlay" -> {
                    try {
                        startActivity(Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                            Uri.parse("package:$packageName")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                    } catch (_: Exception) {}
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

// ============================================================
//  Nghe THONG BAO CUOC GOI (moi app: SIM, Zalo, Mess...)
//  - Cuoc goi den = thong bao category CALL co nut "Nghe".
//  - Lay ten nguoi goi + nut Nghe/Tu choi, gui len dong ho.
//  - Dong ho bam nut -> ban lai dung nut Nghe/Tu choi cua app do.
// ============================================================
class CallListener : NotificationListenerService() {
    companion object {
        private var answerPI: PendingIntent? = null
        private var declinePI: PendingIntent? = null
        private var currentKey: String? = null
        private val lastSent = HashMap<String, String>()   // key -> "title|text" gan nhat (chong dội)

        fun answer() { try { answerPI?.send() } catch (_: Exception) {} }
        fun reject() { try { declinePI?.send() } catch (_: Exception) {} }
    }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val n = sbn.notification ?: return
        if (n.category == Notification.CATEGORY_CALL) { handleCall(sbn, n); return }
        // Thong bao thuong (tin nhan Zalo/Mess/SMS/email...) -> chuyen tiep len dong ho
        if (sbn.packageName == packageName) return
        if ((n.flags and Notification.FLAG_GROUP_SUMMARY) != 0) return   // bo thong bao gop
        // Bo thong bao THUONG TRU (thanh tai, dieu huong... cap nhat lien tuc lam nghen BLE),
        // tru thong bao nhac (MediaStyle) thi van giu de hien man Nhac.
        val isMedia = (n.extras.getString(Notification.EXTRA_TEMPLATE) ?: "").contains("MediaStyle")
        if ((n.flags and Notification.FLAG_ONGOING_EVENT) != 0 && !isMedia) return
        val ex = n.extras
        val title = ex.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        var text = ex.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""
        if (text.isEmpty()) text = ex.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString() ?: ""
        if (text.isEmpty()) {
            val lines = ex.getCharSequenceArray(Notification.EXTRA_TEXT_LINES)
            if (lines != null && lines.isNotEmpty()) text = lines.last().toString()
        }
        if (title.isEmpty() && text.isEmpty()) return
        // Chong dội: cùng noi dung -> khong gui lai (nhieu app cap nhat thong bao lien tuc)
        val sig = "$title|$text"
        if (lastSent[sbn.key] == sig) return
        lastSent[sbn.key] = sig
        if (lastSent.size > 60) lastSent.clear()
        notifyFlutter("notification", mapOf(
            "pkg" to sbn.packageName, "title" to title, "text" to text, "removed" to false))
    }

    private fun handleCall(sbn: StatusBarNotification, n: Notification) {
        val actions = n.actions ?: return
        var ans: PendingIntent? = null
        var dec: PendingIntent? = null
        for (a in actions) {
            val t = (a.title ?: "").toString().lowercase()
            if (t.contains("answer") || t.contains("tra loi") || t.contains("trả lời") ||
                t.contains("nghe") || t.contains("accept") || t.contains("chap nhan") ||
                t.contains("chấp nhận") || t.contains("bắt máy") || t.contains("bat may"))
                ans = a.actionIntent
            else if (t.contains("decline") || t.contains("reject") || t.contains("tu choi") ||
                t.contains("từ chối") || t.contains("hang") || t.contains("end") ||
                t.contains("dismiss") || t.contains("ignore") || t.contains("ket thuc"))
                dec = a.actionIntent
        }
        if (ans == null) return   // khong co nut Nghe -> chua phai cuoc goi den do chuong

        answerPI = ans
        declinePI = dec ?: if (actions.isNotEmpty()) actions[0].actionIntent else null
        currentKey = sbn.key

        val title = n.extras.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: "Cuoc goi den"
        notifyFlutter("incomingCall", mapOf("name" to title, "app" to appLabel(sbn.packageName)))
    }

    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        if (sbn.key == currentKey) {
            currentKey = null; answerPI = null; declinePI = null
            notifyFlutter("callEnded", null)
            return
        }
        if (sbn.packageName == packageName) return
        if (lastSent.remove(sbn.key) == null) return   // chua tung gui -> khong bao xoa
        notifyFlutter("notification", mapOf(
            "pkg" to sbn.packageName, "title" to "", "text" to "", "removed" to true))
    }

    private fun appLabel(pkg: String): String = try {
        val pm = packageManager
        pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString()
    } catch (_: Exception) { pkg }

    private fun notifyFlutter(method: String, args: Any?) {
        Handler(Looper.getMainLooper()).post {
            try { MainActivity.mediaChannel?.invokeMethod(method, args) } catch (_: Exception) {}
        }
    }
}
