# Keyboard Recon — BLE Identity Scanner

[![CI](https://github.com/savagedamage/keyboard-recon/actions/workflows/ci.yml/badge.svg)](https://github.com/savagedamage/keyboard-recon/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

**Find out what a wireless keyboard calls itself before you ever interact with it.**

A keyboard is a computer: an MCU, firmware, a radio, an update path. Hosts trust
its self-declared identity, and identity is cheap to spoof. This is a small,
read-only firmware utility for an ESP32-S3 that passively records what a BLE
keyboard (or any BLE device) advertises when it powers on — so you can answer
*"who is this thing and what radios does it have?"* from evidence, not guesses.

It is the **first-stage reconnaissance** step of a broader peripheral-analysis
workflow. It never pairs, connects, reads GATT, or writes anything; it only
listens to advertisements already on the air. Once you have the advertised
identity, take the host-side follow-up with the companion
[`keyboard-security-analysis`](https://github.com/savagedamage/hid-security-research)
methodology (GATT tree, PnP ID vs manufacturer-name spoof check, idle-injection
test).

> **Current status: alpha firmware.** The decision logic in
> [`include/ble_logic.hpp`](include/ble_logic.hpp) is pure, host-testable C++
> covered by a native unit-test suite, and the full firmware build plus those
> tests run in GitHub Actions CI (see [Build and CI](#build-and-ci)). The ESP32
> sketch (`src/ble_scanner.ino`) targets the standard Arduino BLE API and is
> compiled by CI for the `esp32-s3-devkitc-1` board; confirm it flashes on your
> own hardware. See [Hardware scope and limits](#hardware-scope-and-limits).

## Why this exists

Bluetooth keyboards broadcast their identity in plaintext long before (and often
regardless of) any encryption. That advertisement is a factual, on-the-air record
of:

- the **LE address** the device is using — and whether it keeps changing it;
- the **advertised local name** — which anyone can set;
- **manufacturer-specific data** and the Bluetooth SIG **Company ID** that
  identifies the radio vendor;
- the **service UUIDs** it wants to be known by.

None of that is proof of malice — a cheap consumer board reuses brand VIDs, and a
legitimate one uses a rotating privacy address. But collecting it dispassionately,
on your own hardware, is what lets you tell *"unusual for a keyboard"* from
*"normal"* with a record instead of a hunch. Read-only observation is the safe
first move.

## What it does

| Feature | Behavior |
| --- | --- |
| Discovery | Passive BLE scan, 10 s windows, looping forever so you can power-cycle the keyboard. |
| Identity fields | Address + RSSI, advertised name, manufacturer data (raw hex), service UUIDs. |
| New/changed detection | A device is printed on first sighting (flagged `**NEW**`) and again only if it later *gains* a name, mfg data, or service we had not seen — no flood of repeated lines. |
| Company ID | Decodes a small set of well-known SIG Company IDs; everything else prints as a raw `CID 0xXXXX` to resolve on the SIG list. |
| Optional filter | Set `NAME_FILTER` to only report devices whose advertised name contains a substring. |

## Build and CI

Requires [PlatformIO](https://platformio.org) (or the Arduino IDE with the
Espressif ESP32 core).

```bash
pio run             # compile the firmware for esp32-s3-devkitc-1
pio run -t upload   # flash over USB (USB CDC on boot enabled)
pio device monitor  # serial @ 115200
pio test -e native  # run the host-native logic unit tests (Unity)
```

The firmware sketch (`src/ble_scanner.ino`) and the pure logic
(`include/ble_logic.hpp`) are built and tested in GitHub Actions CI — see the
badge above. The logic is decoupled from the BLE/hardware layer so it runs as an
ordinary host test; the ESP32 build is verified in CI because PlatformIO's tool
package mirror was unreachable from the authoring environment.

Then power on the keyboard and watch the stream. Each window ends with a summary
line (`adverts this window` / `unique this session`).

## Reading the output

```
 3631 ms  RSSI -59  **NEW**  D6:7E:3A:9B:12:40  [name: XZ-7 Keyboard]  mfg[10]: 02 00 00 00 00 00 00 00 00 00  [Microsoft]  svc: 1812
```

- **`**NEW**`** — first time this address appeared since power-on/reset.
- **`RSSI`** — signal strength; a stable value means the same physical device.
- **`mfg[...]`** — raw manufacturer payload; the first two bytes are the
  little-endian SIG Company ID. A decoded name (e.g. `[Microsoft]`) means the
  radio vendor is known; `CID 0xXXXX` means resolve it on the assigned-numbers
  list.
- **`svc:`** — advertised GATT service UUIDs. `0x1812` is Human Interface Device
  (HID); any additional UUIDs are worth a second look.
- **The address.** A consumer keyboard is expected to keep a stable public
  address, or a resolvable private address that stays stable once paired (via the
  identity resolving key). A device that mints a **fresh non-resolvable address
  every burst** — or worse, one whose addresses **increment monotonically** — is
  atypical and worth recording as a finding. Capture several windows before
  deciding anything.

## Hardware scope and limits

- **ESP32-S3 is BLE-only.** It has no Classic Bluetooth (BR/EDR) radio, so it
  cannot confirm whether a keyboard *also* talks Classic BT. For that you need a
  BR/EDR-capable controller — an original dual-mode ESP32, or a Classic-capable
  host controller (on Linux `bluetoothctl scan on` lists Classic devices and
  `btmon` captures their HCI).
- **Not every wireless keyboard is Bluetooth.** Many are proprietary 2.4 GHz
  (nRF24 / a vendor "gaming dongle") or tri-mode. This utility sees only BLE;
  handle the others with a 2.4 GHz capture path.
- **Plaintext, not decrypted.** This reads advertisement headers, not encrypted
  key reports. It tells you who the device *claims* to be, not what it types.
- **A name/VID match is not a verdict.** Identity enumeration is evidence for a
  report, not a malware conclusion. See the first-stage/ID-spoof distinction in
  the companion methodology.

## Companion OS-side workflow

The advertised identity is only half the picture. On a Linux host the same device
drives a far richer readout — the GATT tree and PnP ID (0x2A50) versus
Manufacturer Name (0x2A29) to detect identity spoofing, the Report Map (0x2A4B)
to enumerate declared HID capabilities, and an idle-injection test to distinguish
an actual covert keyboard from a normal-looking one. That methodology, its
capture discipline, and its evidence-preservation rules live in the
[`hid-security-research`](https://github.com/savagedamage/hid-security-research)
repo — this firmware feeds it. The related `keyboard-security-analysis` workflow
covers packet-level capture and the malicious/benign idle-injection test.

## Scope of use

An observation tool for radios and devices in the physical environment. Listen
only to what is already being transmitted, and use the readout to describe and
investigate hardware you can physically account for. Keep the result as recorded
observations plus a clear audit trail — not as a premise for an escalated exploit.

## License

MIT. See [LICENSE](LICENSE).
