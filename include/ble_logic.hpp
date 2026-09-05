// ble_logic.hpp — pure C++ BLE identity logic (no hardware, no Arduino, no BLE).
// ---------------------------------------------------------------------------
// The scanner's non-hardware decisions live here so they can be unit-tested on a
// host with a plain C++ compiler. This header must stay dependency-free: it uses
// only the C++ standard library. Do NOT add <BLEDevice.h>, <Arduino.h>,
// <String>, or any ESP32/BLE include here.
//
// Everything is defined inline so that including this header is sufficient; there
// is no separate implementation to compile.
// ---------------------------------------------------------------------------
#pragma once

#include <cstddef>
#include <cstdint>
#include <cctype>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace ble_recon {

// Bluetooth SIG Company ID: the first two bytes (little-endian) of
// manufacturer-specific data. Returns 0xFFFF ("unknown / not assigned") when the
// buffer holds fewer than two bytes, matching the SIG placeholder.
inline uint16_t company_id_from_mfg(const uint8_t* mfg, size_t len) {
  if (mfg == nullptr || len < 2) return 0xFFFF;
  return static_cast<uint16_t>(static_cast<uint16_t>(mfg[1]) << 8 | mfg[0]);
}

// Human-readable name for a small set of well-known SIG Company IDs. Returns
// nullptr when the ID is not decoded here; callers should then print a raw
// `CID 0xXXXX` so it can be resolved against the live assigned-numbers list.
inline const char* company_id_name(uint16_t cid) {
  switch (cid) {
    case 0x0002: return "Microsoft";
    case 0x0005: return "Samsung";
    case 0x004C: return "Apple";
    case 0x0059: return "Nordic Semiconductor";
    default:     return nullptr;  // not decoded; print raw CID
  }
}

// True when `name` contains `filter` as a case-insensitive substring. An empty
// `filter` matches everything (disabled filter).
inline bool name_matches(const std::string& name, const std::string& filter) {
  if (filter.empty()) return true;
  std::string n = name, f = filter;
  for (std::string::size_type i = 0; i < n.size(); ++i)
    n[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(n[i])));
  for (std::string::size_type i = 0; i < f.size(); ++i)
    f[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(f[i])));
  return n.find(f) != std::string::npos;
}

// Per-device state so we only re-report something when a device GAINS a new
// field after it first appeared, instead of re-flooding the same line forever.
struct DeviceState {
  bool printedName = false;
  bool printedMfg  = false;
  bool printedSvc  = false;
};

// Result of a decision pass: whether to emit a line at all (`show`), whether it
// is a first sighting (`isNew`), and which fields should be printed.
struct PrintDecision {
  bool show      = false;
  bool isNew     = false;
  bool showName  = false;
  bool showMfg   = false;
  bool showSvc   = false;
};

// Decide what, if anything, to report for a device. `st` is mutated only when a
// field is actually shown, so a field is printed once until the device regresses
// and regains it (a fresh DeviceState would reset the flags).
//
//   - show    = isNew, or at least one field is newly present
//               (hasName && !printedName, etc.).
//   - showName/showMfg/showSvc are returned for the (has* && newly present)
//     fields only; the corresponding printed* flag is set when shown.
//   - A brand-new device with no fields still results in show == true (an
//     "**NEW**" line with nothing to append), exactly like the original sketch.
inline PrintDecision decide(bool isNew, bool hasName, bool hasMfg, bool hasSvc,
                            DeviceState& st) {
  PrintDecision d;
  d.isNew   = isNew;
  d.showName = (isNew || !st.printedName) && hasName;
  d.showMfg  = (isNew || !st.printedMfg)  && hasMfg;
  d.showSvc  = (isNew || !st.printedSvc)  && hasSvc;
  d.show = isNew || (hasName && !st.printedName) ||
                   (hasMfg && !st.printedMfg) ||
                   (hasSvc && !st.printedSvc);

  if (d.showName) st.printedName = true;
  if (d.showMfg)  st.printedMfg  = true;
  if (d.showSvc)  st.printedSvc  = true;
  return d;
}

// Session-scoped tracker of which LE addresses have been seen. Keeps the count
// of unique addresses reported so far, independent of per-device field state.
class AddressRegistry {
 public:
  // True the first time `addr` is seen; false on any later call.
  bool is_new(const std::string& addr) {
    return seen_.insert(addr).second;
  }

  // Number of distinct addresses seen this session.
  std::size_t unique_count() const {
    return seen_.size();
  }

 private:
  std::set<std::string> seen_;
};

// Build the full, human-readable observation line emitted per report. Keeping
// this in the header (instead of the sketch) is what makes the on-screen output
// deterministic and unit-testable. `name` is the advertised name (empty if
// none), `mfg` the raw manufacturer-data bytes (empty if none), `svcs` the
// advertised GATT service UUIDs (empty if none). The frame is fixed-width so
// the live stream stays scannable when many devices appear.
inline std::string format_observation(uint32_t ms, int rssi,
    const std::string& addr, bool isNew, const std::string& name,
    const std::vector<uint8_t>& mfg, const std::vector<std::string>& svcs) {
  char head[96];
  std::snprintf(head, sizeof(head), "%8lu ms  RSSI %4d%s  %s",
                (unsigned long)ms, rssi, isNew ? "  **NEW**" : "", addr.c_str());
  std::string out(head);
  if (!name.empty()) { out += "  [name: "; out += name; out += "]"; }
  if (!mfg.empty()) {
    out += "  mfg[" + std::to_string(mfg.size()) + "]:";
    char b[6];
    for (uint8_t byte : mfg) {
      std::snprintf(b, sizeof(b), " %02X", (unsigned)byte);
      out += b;
    }
    uint16_t cid = company_id_from_mfg(mfg.data(), mfg.size());
    const char* cn = company_id_name(cid);
    if (cn) { out += "  ["; out += cn; out += "]"; }
    else {
      char cidb[24];
      std::snprintf(cidb, sizeof(cidb), "  [CID 0x%04X]", cid);
      out += cidb;
    }
  }
  if (!svcs.empty()) {
    out += "  svc:";
    for (const auto& svc : svcs) { out += " "; out += svc; }
  }
  return out;
}

// The line printed at the end of each 10 s scan window. `adverts` is the number
// of advertisements seen in that window, `unique` the distinct addresses seen
// so far this session.
inline std::string format_window_summary(int adverts, std::size_t unique) {
  char b[160];
  std::snprintf(b, sizeof(b),
                "--- window done: %d adverts this window, %zu unique this session ---",
                adverts, unique);
  return b;
}

}  // namespace ble_recon
