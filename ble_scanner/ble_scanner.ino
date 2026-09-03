// BLE discovery scanner — ESP32-S3 (Arduino core)
// Purpose: identify the malicious keyboard's advertised name, MAC, manufacturer data,
//          and service UUIDs, and confirm whether it's BLE vs classic BT.
// Flash with Arduino IDE / PlatformIO (board: ESP32-S3 Dev Module, USB CDC on boot enabled).
// Open serial monitor @ 115200, then power on the keyboard and watch the stream.

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

class AdvCB : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    Serial.printf("RSSI %d  %s", d.getRSSI(), d.getAddress().toString().c_str());
    if (d.haveName()) Serial.printf("  [name: %s]", d.getName().c_str());
    if (d.haveManufacturerData()) {
      std::string m = d.getManufacturerData();
      Serial.printf("  mfg[%d]:", (int)m.length());
      for (size_t i = 0; i < m.length(); i++) Serial.printf(" %02X", (uint8_t)m[i]);
    }
    if (d.getServiceUUIDCount()) {
      Serial.print("  svc:");
      for (int i = 0; i < d.getServiceUUIDCount(); i++)
        Serial.printf(" %s", d.getServiceUUID(i).toString().c_str());
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
  pScan->setActiveScan(true);   // request scan responses (gets name/extra data)
  pScan->setInterval(100);
  pScan->setWindow(99);
  Serial.println("BLE scan running. Power on the keyboard and watch for its identity.");
}

void loop() {
  // 10-second active scan windows, repeated forever so you can power-cycle the keyboard.
  BLEScanResults r = pScan->start(10, false);
  Serial.printf("--- 10s window done, %d unique adverts ---\n", r.getCount());
  delay(200);
}
