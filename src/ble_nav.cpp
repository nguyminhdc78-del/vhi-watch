#include "ble_nav.h"
#include "config.h"
#include "app_state.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>

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

    svc->start();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setScanResponse(true);
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE] Bat dau quang cao");
}

void ble_notify_status() {
    if (!chrStatus || !g_sys.bleConnected) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"batt\":%d,\"nav\":%d}",
             g_sys.battPercent, g_nav.active ? 1 : 0);
    chrStatus->setValue((uint8_t *)buf, strlen(buf));
    chrStatus->notify();
}
