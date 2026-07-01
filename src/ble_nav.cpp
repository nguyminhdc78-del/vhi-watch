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
        StaticJsonDocument<320> doc;
        if (deserializeJson(doc, v.c_str()) != DeserializationError::Ok) return;
        strlcpy(g_notify.app,   doc["app"]   | "", sizeof(g_notify.app));
        strlcpy(g_notify.title, doc["title"] | "", sizeof(g_notify.title));
        strlcpy(g_notify.text,  doc["text"]  | "", sizeof(g_notify.text));
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

static NimBLECharacteristic *chrMedia = nullptr;

void ble_init() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCB());

    NimBLEService *svc = server->createService(BLE_SVC_UUID);

    NimBLECharacteristic *chrNav =
        svc->createCharacteristic(BLE_CHR_NAV_UUID, NIMBLE_PROPERTY::WRITE);
    chrNav->setCallbacks(new NavCB());

    NimBLECharacteristic *chrTime =
        svc->createCharacteristic(BLE_CHR_TIME_UUID, NIMBLE_PROPERTY::WRITE);
    chrTime->setCallbacks(new TimeCB());

    chrStatus = svc->createCharacteristic(
        BLE_CHR_STATUS_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *chrWp =
        svc->createCharacteristic(BLE_CHR_WP_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrWp->setCallbacks(new WpCB());

    NimBLECharacteristic *chrRoute =
        svc->createCharacteristic(BLE_CHR_ROUTE_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrRoute->setCallbacks(new RouteCB());

    NimBLECharacteristic *chrNotify =
        svc->createCharacteristic(BLE_CHR_NOTIFY_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrNotify->setCallbacks(new NotifyCB());

    NimBLECharacteristic *chrMusic =
        svc->createCharacteristic(BLE_CHR_MUSIC_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrMusic->setCallbacks(new MusicCB());

    chrMedia = svc->createCharacteristic(
        BLE_CHR_MEDIA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *chrColor =
        svc->createCharacteristic(BLE_CHR_COLOR_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrColor->setCallbacks(new ColorCB());

    NimBLECharacteristic *chrImgSel =
        svc->createCharacteristic(BLE_CHR_IMGSEL_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrImgSel->setCallbacks(new ImgSelCB());

    NimBLECharacteristic *chrWeather =
        svc->createCharacteristic(BLE_CHR_WEATHER_UUID,
                                  NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    chrWeather->setCallbacks(new WeatherCB());

    svc->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Bat dau quang cao");
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
