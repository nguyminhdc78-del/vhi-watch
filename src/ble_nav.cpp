#include "ble_nav.h"
#include "config.h"
#include "app_state.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

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

        if (wpReceived == 0) {                    // chunk dau -> mo file moi
            wpFile = LittleFS.open(WALLPAPER_PATH, "w");
            if (!wpFile) { Serial.println("[WP] Khong mo duoc file"); return; }
        }
        if (!wpFile) return;

        wpFile.write((const uint8_t *)v.data(), len);
        wpReceived += len;

        if (wpReceived >= WALLPAPER_BYTES) {      // nhan du -> xong
            wpFile.close();
            wpReceived = 0;
            g_wpUpdated = true;                   // UI se nap lai o vong loop
            Serial.println("[WP] Nhan xong anh nen");
        }
    }
};

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
    Serial.println("[BLE] Da tat de nhuong song cho WiFi");
}

void ble_notify_status() {
    if (!chrStatus || !g_sys.bleConnected) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"batt\":%d,\"nav\":%d}",
             g_sys.battPercent, g_nav.active ? 1 : 0);
    chrStatus->setValue((uint8_t *)buf, strlen(buf));
    chrStatus->notify();
}
