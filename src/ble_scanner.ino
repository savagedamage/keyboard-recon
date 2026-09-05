// BLE identity scanner — ESP32-S3 (Arduino core)
// ---------------------------------------------------------------------------
// Purpose: catch the advertised identity of a keyboard (or any BLE device) as it
// powers on, so you learn what it calls itself before you interact with it:
//   - Bluetooth LE address (and whether it rotates between windows)
//   - advertised local name
//   - manufacturer-specific data (raw bytes + vendor Company ID)
//   - advertised service UUIDs
//
// Defensive, read-only: this sketch never pairs, connects, or writes. It only
// listens to advertisements already on the air.
//
// HARDWARE SCOPE (read before assuming coverage):
//   * ESP32-S3 is Bluetooth LE ONLY. It has no BR/EDR ("Classic Bluetooth")
//     radio, so it cannot confirm whether a device ALSO does Classic BT.
//   * For Classic BT discovery you need a BR/EDR-capable controller (an
//     original dual-mode ESP32, or any Classic-capable host controller — on
//     Linux: `bluetoothctl scan on`, or capture HCI with `btmon`).
//   * Many "wireless keyboards" are NOT Bluetooth at all: proprietary 2.4 GHz
//     (nRF24 / vendor "gaming dongle") or tri-mode. This sketch does not touch
//     those; use the repo's notes on 2.4 GHz capture instead.
//
// Flash with Arduino IDE or PlatformIO (board esp32-s3-devkitc-1, "USB CDC on
// boot" enabled). Serial monitor @ 115200. Power on the keyboard and watch.
//
// State: kept only in RAM. A reset clears the "seen" set, so a fresh keyboard
// powers on and gets flagged [NEW] again.
//
// All decision + formatting logic lives in include/ble_logic.hpp as pure C++ so
// it can be unit-tested on a host. This sketch is a thin BLE-callback shell: it
// gathers the advertisement fields, calls ble_recon, and prints what it returns.
// ---------------------------------------------------------------------------

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>
#include <string>
#include <vector>

#include "ble_logic.hpp"

// Optional: only report devices whose advertised name contains this substring.
// Leave empty ("") to see everything. Matched case-insensitively.
const char* NAME_FILTER = "";

// Per-address advertised-field state + session uniqueness.
std::map<std::string, ble_recon::DeviceState> g_state;
ble_recon::AddressRegistry g_registry;

class AdvCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    std::string addr = d.getAddress().toString().c_str();
    const bool isNew = g_registry.is_new(addr);
    ble_recon::DeviceState &st = g_state[addr];

    // Optional name filter (case-insensitive substring on the advertised name,
    // or the address when no name is advertised).
    if (NAME_FILTER[0] != '\0') {
      std::string name;
      if (d.haveName()) name = d.getName().c_str(); else name = addr;
      if (!ble_recon::name_matches(name, NAME_FILTER)) return;  // filtered out
    }

    const bool hasName = d.haveName();
    const bool hasMfg  = d.haveManufacturerData();
    const bool hasSvc  = d.getServiceUUIDCount() > 0;

    ble_recon::PrintDecision p =
        ble_recon::decide(isNew, hasName, hasMfg, hasSvc, st);
    if (!p.show) return;  // nothing new to report

    // Gather only the fields we decided to show, then build one line.
    std::string name;
    std::vector<uint8_t> mfg;
    std::vector<std::string> svcs;
    if (p.showName && hasName) name = d.getName().c_str();
    if (p.showMfg  && hasMfg) {
      std::string md = d.getManufacturerData();
      mfg.assign(md.begin(), md.end());
    }
    if (p.showSvc  && hasSvc) {
      for (int i = 0; i < d.getServiceUUIDCount(); i++)
        svcs.push_back(d.getServiceUUID(i).toString().c_str());
    }

    Serial.println(ble_recon::format_observation(
        (uint32_t)millis(), d.getRSSI(), addr, isNew, name, mfg, svcs).c_str());
  }
};

BLEScan *pScan;

void setup() {
  Serial.begin(115200);
  delay(200);
  BLEDevice::init("");
  pScan = BLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvCB());
  pScan->setActiveScan(true);     // request scan responses (gets name / extra data)
  pScan->setInterval(100);
  pScan->setWindow(99);

  Serial.println();
  Serial.println("=== Keyboard Recon — BLE identity scanner ===");
  Serial.println("Passive BLE observation: never pairs, connects, or writes.");
  Serial.println("ESP32-S3 is BLE-only (no Classic BT).");
  Serial.print  ("Name filter: ");
  Serial.println(NAME_FILTER[0] != '\0' ? NAME_FILTER : "none (seeing all devices)");
  Serial.println("Power on the keyboard and watch its advertised identity stream below.");
  Serial.println();
}

void loop() {
  // Blocking 10s window, repeated forever so you can power-cycle the keyboard.
  BLEScanResults r = pScan->start(10, false);
  Serial.println(ble_recon::format_window_summary(
      r.getCount(), (std::size_t)g_registry.unique_count()).c_str());
  delay(200);
}
