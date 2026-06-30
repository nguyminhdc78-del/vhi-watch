#include "hid_remote.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

// ============================================================
//  HID Keyboard qua BLE (NimBLE) - dong ho lam remote cho PC
// ============================================================
static NimBLEHIDDevice     *hid      = nullptr;
static NimBLECharacteristic *kbdInput = nullptr;   // Report ID 1: ban phim
static NimBLECharacteristic *ccInput  = nullptr;   // Report ID 2: consumer (volume)
static bool                 connected = false;

// Report map: ban phim (ID 1) + consumer control (ID 2)
static const uint8_t REPORT_MAP[] = {
    // --- Ban phim (Report ID 1) ---
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  //   Report ID (1)
    0x05, 0x07,  //   Usage Page (Keyboard)
    0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01,
    0x75, 0x01, 0x95, 0x08, 0x81, 0x02,  // modifier byte
    0x95, 0x01, 0x75, 0x08, 0x81, 0x01,  // reserved
    0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,  // 6 phim
    0xC0,        // End Collection

    // --- Consumer Control (Report ID 2): 1 usage 16-bit ---
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x02,        //   Report ID (2)
    0x15, 0x00,        //   Logical Min (0)
    0x26, 0xFF, 0x03,  //   Logical Max (1023)
    0x19, 0x00,        //   Usage Min (0)
    0x2A, 0xFF, 0x03,  //   Usage Max (1023)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data,Array)
    0xC0               // End Collection
};

class HidServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s) override    { connected = true; }
    void onDisconnect(NimBLEServer *s) override { connected = false; NimBLEDevice::startAdvertising(); }
};

void hid_start() {
    NimBLEDevice::init("VHI-Remote");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, false, false);          // bonding, just-works
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new HidServerCB());

    hid = new NimBLEHIDDevice(server);
    kbdInput = hid->inputReport(1);                              // Report ID 1: ban phim
    ccInput  = hid->inputReport(2);                              // Report ID 2: consumer
    hid->manufacturer()->setValue("VHI");
    hid->pnp(0x02, 0xE502, 0xA111, 0x0210);
    hid->hidInfo(0x00, 0x01);
    hid->reportMap((uint8_t *)REPORT_MAP, sizeof(REPORT_MAP));
    hid->startServices();

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(HID_KEYBOARD);
    adv->addServiceUUID(hid->hidService()->getUUID());
    adv->start();
    Serial.println("[HID] Remote dang quang cao (VHI-Remote)");
}

void hid_stop() {
    NimBLEDevice::deinit(true);
    hid = nullptr;
    kbdInput = nullptr;
    ccInput = nullptr;
    connected = false;
}

bool hid_connected() { return connected; }

void hid_send_key(uint8_t keycode) {
    if (!connected || !kbdInput) return;
    uint8_t press[8]   = {0, 0, keycode, 0, 0, 0, 0, 0};
    uint8_t release[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    kbdInput->setValue(press, 8);   kbdInput->notify();
    delay(8);
    kbdInput->setValue(release, 8); kbdInput->notify();
}

void hid_send_consumer(uint16_t usage) {
    if (!connected || !ccInput) return;
    uint8_t press[2]   = {(uint8_t)(usage & 0xFF), (uint8_t)(usage >> 8)};
    uint8_t release[2] = {0, 0};
    ccInput->setValue(press, 2);   ccInput->notify();
    delay(8);
    ccInput->setValue(release, 2); ccInput->notify();
}
