#include "ble_nav.h"
#include "config.h"
#include "app_state.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "storage.h"

// --- Nhan anh nen qua BLE: phone stream dung WALLPAPER_BYTES (115200) byte RGB565 ---
static File     wpFile;
static uint32_t wpReceived = 0;
static void wp_reset() { if (wpFile) wpFile.close(); wpReceived = 0; }

// ============================================================
//  Goi tin nav (JSON) tu app dien thoai, vi du:
//  {"m":2,"d":150,"s":"Re phai vao Le Loi","e":"12:34","r":3200}
//   m = maneuver (enum NavManeuver)
//   d = distance toi diem re (met)
//   s = street/huong dan
//   e = ETA
//   r = tong quang duong con lai (met)
//  Goi {"sync":1719600000} de dong bo thoi gian (unix epoch giay)
// ============================================================

static NimBLECharacteristic *chrStatus = nullptr;

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s) override {
        g_sys.bleConnected = true;
        Serial.println("[BLE] Dien thoai da ket noi");
        // KHONG ep supervision timeout ngan (an-ten yeu -> hay rot). De mac dinh cho on dinh.
    }
    void onDisconnect(NimBLEServer *s) override {
        g_sys.bleConnected = false;
        wp_reset();   // huy transfer anh dang do (neu co)
        Serial.println("[BLE] Ngat ket noi - quang cao lai");
        NimBLEDevice::startAdvertising();
    }
};

// Xu ly goi nav
class NavCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;

        if (doc.containsKey("stop")) {            // dung dan duong
            g_nav.active = false;
            return;
        }
        g_nav.active     = true;
        g_nav.maneuver   = (NavManeuver)(doc["m"] | 0);
        g_nav.distance_m = doc["d"] | 0;
        g_nav.remain_m   = doc["r"] | 0;
        strlcpy(g_nav.street, doc["s"] | "", sizeof(g_nav.street));
        strlcpy(g_nav.eta,    doc["e"] | "", sizeof(g_nav.eta));
        g_nav.lastUpdateMs = millis();
    }
};

// Xu ly dong bo thoi gian
class TimeCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        uint32_t epoch = strtoul(v.c_str(), nullptr, 10);
        if (epoch > 1700000000UL) {               // hop le (sau 2023)
            clock_sync(epoch);
            Serial.printf("[BLE] Dong bo thoi gian: %lu\n", epoch);
        }
    }
};

// Nhan anh nen (RGB565) theo tung chunk, ghi vao LittleFS.
// Phone gui dung WALLPAPER_BYTES byte; khi du -> dong file + bao UI nap lai.
class WpCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        size_t len = v.size();
        if (len == 0) return;

        if (wpReceived == 0) {                    // chunk dau -> mo file theo dich upload
            char path[20];
            if (g_uploadTarget >= QR_COUNT) strcpy(path, WALLPAPER_PATH);          // 0xFF = anh nen
            else snprintf(path, sizeof(path), QR_IMG_PATH_FMT, g_uploadTarget);     // 0..N = o QR
            wpFile = LittleFS.open(path, "w");
            if (!wpFile) { Serial.println("[WP] Khong mo duoc file"); return; }
        }
        if (!wpFile) return;

        wpFile.write((const uint8_t *)v.data(), len);
        wpReceived += len;

        if (wpReceived >= WALLPAPER_BYTES) {      // nhan du -> xong
            wpFile.close();
            wpReceived = 0;
            if (g_uploadTarget >= QR_COUNT) g_wpUpdated = true;   // anh nen
            else g_qrImgUpdated = true;                          // anh QR
            Serial.println("[WP] Nhan xong anh");
        }
    }
};

// Nhan duong line lo trinh: byte[0] = so diem, sau do la cac cap (x,y) int8
class RouteCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        const uint8_t *d = (const uint8_t *)v.data();
        size_t len = v.size();
        if (len < 1) return;
        int n = d[0];
        int avail = (int)(len - 1) / 2;
        if (n > avail) n = avail;
        if (n > MAX_ROUTE_PTS) n = MAX_ROUTE_PTS;
        for (int i = 0; i < n; i++) {
            g_routeXY[i * 2]     = (int8_t)d[1 + i * 2];
            g_routeXY[i * 2 + 1] = (int8_t)d[1 + i * 2 + 1];
        }
        g_routeCount = n;
        g_routeDirty = true;
    }
};

// Thong bao tu dien thoai: {"app":"...","title":"...","text":"..."}
class NotifyCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        StaticJsonDocument<512> doc;   // du cho thong bao tieng Viet dai (UTF-8 nhieu byte)
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;
        strlcpy(g_notify.app,   doc["app"]   | "", sizeof(g_notify.app));
        strlcpy(g_notify.title, doc["title"] | "", sizeof(g_notify.title));
        strlcpy(g_notify.text,  doc["text"]  | "", sizeof(g_notify.text));
        g_notify.canReply = (doc["r"] | 0) != 0;   // co tra loi nhanh duoc khong
        g_notify.hasNew = true;
        g_lastInputMs = millis();   // giu thuc de doc thong bao
    }
};

// Bai hat dang phat: {"title":"...","artist":"...","playing":1}
class MusicCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;
        strlcpy(g_music.title,  doc["title"]  | "", sizeof(g_music.title));
        strlcpy(g_music.artist, doc["artist"] | "", sizeof(g_music.artist));
        g_music.playing = (doc["playing"] | 0) != 0;
    }
};

// Mau chu: 3 byte R, G, B
class ColorCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.size() >= 3) {
            g_uiR = (uint8_t)v[0];
            g_uiG = (uint8_t)v[1];
            g_uiB = (uint8_t)v[2];
            g_colorChanged = true;
            g_colorSave = true;   // ghi flash o main loop (tranh block callback BLE)
        }
    }
};

// Chon dich upload anh ke tiep: 1 byte (0xFF = anh nen, 0..QR_COUNT-1 = o QR)
class ImgSelCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.size() >= 1) g_uploadTarget = (uint8_t)v[0];
    }
};

// Thoi tiet: {"t":31,"w":"Nang"}
class WeatherCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;
        g_weather.temp = doc["t"] | 0;
        strlcpy(g_weather.text, doc["w"] | "", sizeof(g_weather.text));
        g_weather.has = true;
    }
};

// Cuoc goi den: {"st":1,"name":"Me","app":"Zalo"}  (st=1 dang goi, st=0 ket thuc)
class CallCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;
        bool ring = (doc["st"] | 0) != 0;
        g_call.ringing = ring;
        if (ring) {
            strlcpy(g_call.name, doc["name"] | "", sizeof(g_call.name));
            strlcpy(g_call.app,  doc["app"]  | "", sizeof(g_call.app));
        }
        g_call.changed = true;
        g_lastInputMs = millis();   // giu thuc de thay cuoc goi
    }
};

// Danh ba nhanh: "C" = xoa het; "ten\tso" = them 1 lien he
class ContactsCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.size() == 1 && v[0] == 'C') {          // xoa danh sach (bat dau dong bo)
            g_contacts.count = 0; g_contacts.updated = true; return;
        }
        size_t tab = v.find('\t');
        if (tab == std::string::npos) return;
        if (g_contacts.count >= MAX_CONTACTS) return;
        Contact &ct = g_contacts.items[g_contacts.count];
        strlcpy(ct.name,   v.substr(0, tab).c_str(),  sizeof(ct.name));
        strlcpy(ct.number, v.substr(tab + 1).c_str(), sizeof(ct.number));
        g_contacts.count++;
        g_contacts.updated = true;
    }
};

// Giao dien gio: [pos, timeSize, dateSize, dateShow, dateR, dateG, dateB]
class WfCfgCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.size() >= 2) {
            if ((uint8_t)v[0] <= 2) g_wfPos = (uint8_t)v[0];
            if ((uint8_t)v[1] >= 3 && (uint8_t)v[1] <= 6) g_wfSize = (uint8_t)v[1];
        }
        if (v.size() >= 4) {
            if ((uint8_t)v[2] >= 1 && (uint8_t)v[2] <= 3) g_dateSize = (uint8_t)v[2];
            g_dateShow = (uint8_t)v[3] ? 1 : 0;
        }
        if (v.size() >= 7) {
            g_dateR = (uint8_t)v[4]; g_dateG = (uint8_t)v[5]; g_dateB = (uint8_t)v[6];
        }
        g_wfCfgChanged = true;
    }
};

// Cau tra loi nhanh: "C" = xoa het; "text" = them 1 cau
class ReplyCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *c) override {
        std::string v = c->getValue();
        if (v.size() == 1 && v[0] == 'C') { g_replies.count = 0; g_replies.updated = true; return; }
        if (g_replies.count >= MAX_REPLIES) return;
        strlcpy(g_replies.items[g_replies.count], v.c_str(), sizeof(g_replies.items[0]));
        g_replies.count++;
        g_replies.updated = true;
    }
};

static NimBLECharacteristic *chrMedia = nullptr;

// Cac callback dung 1 lan (static): ble_init() bi goi lai moi lan thoat Remote/Chup hinh.
// NimBLE KHONG bao gio tu delete callback cua characteristic -> neu dung "new" moi lan
// se ro ri RAM tich luy -> het heap -> treo. Dung instance static: tao 1 lan, khong ro ri.
static ServerCB    s_serverCB;
static NavCB       s_navCB;
static TimeCB      s_timeCB;
static WpCB        s_wpCB;
static RouteCB     s_routeCB;
static NotifyCB    s_notifyCB;
static MusicCB     s_musicCB;
static ColorCB     s_colorCB;
static ImgSelCB    s_imgSelCB;
static WeatherCB   s_weatherCB;
static CallCB      s_callCB;
static ContactsCB  s_contactsCB;
static WfCfgCB     s_wfCfgCB;
static ReplyCB     s_replyCB;

void ble_init() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(&s_serverCB, false);   // false = KHONG tu delete (day la object static)

    NimBLEService *svc = server->createService(BLE_SVC_UUID);

    NimBLECharacteristic *chrNav =
        svc->createCharacteristic(BLE_CHR_NAV_UUID, NIMBLE_PROPERTY::WRITE);
    chrNav->setCallbacks(&s_navCB);

    NimBLECharacteristic *chrTime =
        svc->createCharacteristic(BLE_CHR_TIME_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrTime->setCallbacks(&s_timeCB);

    chrStatus = svc->createCharacteristic(
        BLE_CHR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *chrWp =
        svc->createCharacteristic(BLE_CHR_WP_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrWp->setCallbacks(&s_wpCB);

    NimBLECharacteristic *chrRoute =
        svc->createCharacteristic(BLE_CHR_ROUTE_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrRoute->setCallbacks(&s_routeCB);

    NimBLECharacteristic *chrNotify =
        svc->createCharacteristic(BLE_CHR_NOTIFY_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrNotify->setCallbacks(&s_notifyCB);

    NimBLECharacteristic *chrMusic =
        svc->createCharacteristic(BLE_CHR_MUSIC_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrMusic->setCallbacks(&s_musicCB);

    chrMedia = svc->createCharacteristic(
        BLE_CHR_MEDIA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *chrColor =
        svc->createCharacteristic(BLE_CHR_COLOR_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrColor->setCallbacks(&s_colorCB);

    NimBLECharacteristic *chrImgSel =
        svc->createCharacteristic(BLE_CHR_IMGSEL_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrImgSel->setCallbacks(&s_imgSelCB);

    NimBLECharacteristic *chrWeather =
        svc->createCharacteristic(BLE_CHR_WEATHER_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrWeather->setCallbacks(&s_weatherCB);

    NimBLECharacteristic *chrCall =
        svc->createCharacteristic(BLE_CHR_CALL_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrCall->setCallbacks(&s_callCB);

    NimBLECharacteristic *chrContact =
        svc->createCharacteristic(BLE_CHR_CONTACT_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrContact->setCallbacks(&s_contactsCB);

    NimBLECharacteristic *chrWfCfg =
        svc->createCharacteristic(BLE_CHR_WFCFG_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrWfCfg->setCallbacks(&s_wfCfgCB);

    NimBLECharacteristic *chrReply =
        svc->createCharacteristic(BLE_CHR_REPLY_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrReply->setCallbacks(&s_replyCB);

    svc->start();

    // Quang cao gon & chac: TEN nam trong goi chinh (app do theo ten luon thay),
    // UUID dich vu de o scan-response (app cu loc theo UUID van chay).
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);                       // LE General Discoverable + BR/EDR not supported
    advData.setName(BLE_DEVICE_NAME);
    adv->setAdvertisementData(advData);

    NimBLEAdvertisementData scanData;
    scanData.setCompleteServices(NimBLEUUID(BLE_SVC_UUID));
    adv->setScanResponseData(scanData);

    adv->setMinInterval(0x30);                     // ~30ms: de dien thoai bat song nhanh
    adv->setMaxInterval(0x60);                     // ~60ms
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Bat dau quang cao");
}

// Goi dinh ky: neu chua ket noi ma vi ly do nao do ngung quang cao -> phat lai
void ble_ensure_advertising() {
    if (g_sys.bleConnected) return;
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    if (adv && !adv->isAdvertising()) NimBLEDevice::startAdvertising();
}

void ble_stop() {
    NimBLEDevice::deinit(true);   // giai phong controller + RAM cho WiFi
    g_sys.bleConnected = false;
    chrStatus = nullptr;
    chrMedia  = nullptr;
    Serial.println("[BLE] Da tat de nhuong song cho WiFi");
}

// Gui lenh dieu khien nhac len dien thoai: "next" / "prev" / "playpause"
void ble_send_media(const char *cmd) {
    if (!chrMedia || !g_sys.bleConnected) return;
    chrMedia->setValue((uint8_t *)cmd, strlen(cmd));
    chrMedia->notify();
}

void ble_notify_status() {
    if (!chrStatus || !g_sys.bleConnected) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"batt\":%d,\"nav\":%d}",
             g_sys.battPercent, g_nav.active ? 1 : 0);
    chrStatus->setValue((uint8_t *)buf, strlen(buf));
    chrStatus->notify();
}
