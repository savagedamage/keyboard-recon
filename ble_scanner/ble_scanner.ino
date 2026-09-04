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
// ---------------------------------------------------------------------------

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <map>
#include <string>
#include <cctype>

// Optional: only report devices whose advertised name contains this substring.
// Leave empty ("") to see everything. Matched case-insensitively.
const char* NAME_FILTER = "";

// Bluetooth SIG Company IDs are the first two bytes (little-endian) of
// manufacturer-specific data. Only a few are decoded here; everything else is
// printed as a raw CID so you can resolve it against the live
// https://www.bluetooth.com/specifications/assigned-numbers/company-identifiers
// list. (0xFFFF = the SIG's "unknown / not assigned" placeholder.)
static const char* companyName(uint16_t id) {
  switch (id) {
    case 0x0002: return "Microsoft";
    case 0x0005: return "Samsung";
    case 0x004C: return "Apple";
    case 0x0059: return "Nordic Semiconductor";
    // Note: "Shenzhen Yichip", a chip vendor observed reusing brand PnP IDs in
    // the wild, is intentionally NOT given a Company ID here because I could not
    // verify its SIG assignation. If you need it, resolve the raw CID on the SIG
    // list rather than trusting a hardcoded value.
    default:     return nullptr;               // print raw CID; resolve on the SIG list
  }
}

// Per-device state so we only re-report something when a device GAINS a new
// field after it first appeared, instead of re-flooding the same line forever.
struct DeviceState {
  bool printedName = false;
  bool printedMfg  = false;
  bool printedSvc  = false;
};

std::map<std::string, DeviceState> g_state;      // keyed by LE address string

class AdvCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    std::string addr = d.getAddress().toString().c_str();
    const bool isNew = g_state.find(addr) == g_state.end();
    DeviceState &st = g_state[addr];

    // Optional name filter.
    if (NAME_FILTER[0] != '\0') {
      std::string name;
      if (d.haveName()) name = d.getName().c_str(); else name = addr;
      std::string lc = name, lf = NAME_FILTER;
      for (auto &c : lc) c = std::tolower((unsigned char)c);
      for (auto &c : lf) c = std::tolower((unsigned char)c);
      if (lc.find(lf) == std::string::npos) return;   // filtered out
    }

    const bool hasName = d.haveName();
    const bool haveMfg = d.haveManufacturerData();
    const bool showName = isNew || (hasName && !st.printedName);
    const bool showMfg  = isNew || (haveMfg && !st.printedMfg);
    const bool showSvc  = isNew || (d.getServiceUUIDCount() > 0 && !st.printedSvc);
    if (!(showName || showMfg || showSvc)) return;      // nothing new to report

    Serial.printf("%lu ms  RSSI %d%s  %s",
                  (unsigned long)millis(), d.getRSSI(),
                  (isNew ? "  **NEW**" : ""), addr.c_str());

    if (showName && hasName) {
      Serial.printf("  [name: %s]", d.getName().c_str());
      st.printedName = true;
    }
    if (showMfg && haveMfg) {
      std::string m = d.getManufacturerData();
      Serial.printf("  mfg[%d]:", (int)m.length());
      for (size_t i = 0; i < m.length(); i++)
        Serial.printf(" %02X", (uint8_t)m[i]);
      if (m.length() >= 2) {
        uint16_t cid = (uint16_t)(((uint8_t)m[1] << 8) | (uint8_t)m[0]);
        const char *cn = companyName(cid);
        if (cn) Serial.printf("  [%s]", cn);
        else    Serial.printf("  [CID 0x%04X]", cid);
      }
      st.printedMfg = true;
    }
    if (showSvc && d.getServiceUUIDCount()) {
      Serial.print("  svc:");
      for (int i = 0; i < d.getServiceUUIDCount(); i++)
        Serial.printf(" %s", d.getServiceUUID(i).toString().c_str());
      st.printedSvc = true;
    }
    Serial.println();
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
  Serial.println("BLE identity scan running (passive). Power on the keyboard.");
  if (NAME_FILTER[0] != '\0')
    Serial.printf("  filter: *%s*\n", NAME_FILTER);
}

void loop() {
  // Blocking 10s window, repeated forever so you can power-cycle the keyboard.
  BLEScanResults r = pScan->start(10, false);
  Serial.printf("--- window done: %d adverts this window, %d unique this session ---\n",
                r.getCount(), (int)g_state.size());
  delay(200);
}
