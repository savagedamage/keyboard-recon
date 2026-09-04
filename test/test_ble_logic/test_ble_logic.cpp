// Unity host-native tests for include/ble_logic.hpp.
// ---------------------------------------------------------------------------
// Run on the host via PlatformIO's `[env:native]` unit-test target
// (`pio test -e native`). The header is pure C++ (std only), so it compiles with
// no arduino/ESP32/BLE dependencies. The ESP32 firmware sketch (src/) is NOT part
// of this build and cannot be compiled on the host.
// ---------------------------------------------------------------------------

#include <unity.h>

#include "ble_logic.hpp"

#include <string>

using namespace ble_recon;

void setUp() {}
void tearDown() {}

// --- company_id_from_mfg -----------------------------------------------------
void test_company_id_short_returns_unknown() {
  // Fewer than 2 bytes (or null) -> 0xFFFF ("unknown / not assigned").
  const uint8_t one[1] = {0x02};
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, company_id_from_mfg(one, 1));
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, company_id_from_mfg(nullptr, 0));
  TEST_ASSERT_EQUAL_UINT16(0xFFFF, company_id_from_mfg(one, 0));
}

void test_company_id_little_endian() {
  const uint8_t microsoft[2] = {0x02, 0x00};  // Microsoft = 0x0002
  TEST_ASSERT_EQUAL_UINT16(0x0002, company_id_from_mfg(microsoft, 2));
  const uint8_t apple[2] = {0x4C, 0x00};      // Apple = 0x004C
  TEST_ASSERT_EQUAL_UINT16(0x004C, company_id_from_mfg(apple, 2));
  const uint8_t nordic[2] = {0x59, 0x00};     // Nordic = 0x0059
  TEST_ASSERT_EQUAL_UINT16(0x0059, company_id_from_mfg(nordic, 2));
  // Byte order must be little-endian: high byte is the second byte.
  const uint8_t non_zero[2] = {0x34, 0x12};   // -> 0x1234
  TEST_ASSERT_EQUAL_UINT16(0x1234, company_id_from_mfg(non_zero, 2));
}

void test_company_id_name_known_and_unknown() {
  TEST_ASSERT_EQUAL_STRING("Microsoft", company_id_name(0x0002));
  TEST_ASSERT_EQUAL_STRING("Samsung", company_id_name(0x0005));
  TEST_ASSERT_EQUAL_STRING("Apple", company_id_name(0x004C));
  TEST_ASSERT_EQUAL_STRING("Nordic Semiconductor", company_id_name(0x0059));
  // Unknown / not decoded -> nullptr so caller prints raw CID.
  TEST_ASSERT_NULL(company_id_name(0x1234));
  TEST_ASSERT_NULL(company_id_name(0xFFFF));
}

// --- name_matches ------------------------------------------------------------
void test_name_matches_empty_filter_always_true() {
  TEST_ASSERT_TRUE(name_matches("Any Keyboard", ""));
}

void test_name_matches_case_insensitive_substring() {
  TEST_ASSERT_TRUE(name_matches("MX Keys", "mx"));
  TEST_ASSERT_TRUE(name_matches("MX Keys", "KEYS"));
  TEST_ASSERT_TRUE(name_matches("Apple Magic Keyboard", "magic keyboard"));
}

void test_name_matches_non_match() {
  TEST_ASSERT_FALSE(name_matches("MX Keys", "logitech"));
  TEST_ASSERT_FALSE(name_matches("MX Keys", "apple"));
}

// --- decide ------------------------------------------------------------------
void test_decide_new_device_all_fields() {
  DeviceState st;
  PrintDecision p = decide(true, true, true, true, st);
  TEST_ASSERT_TRUE(p.show);
  TEST_ASSERT_TRUE(p.isNew);
  TEST_ASSERT_TRUE(p.showName);
  TEST_ASSERT_TRUE(p.showMfg);
  TEST_ASSERT_TRUE(p.showSvc);
  TEST_ASSERT_TRUE(st.printedName);
  TEST_ASSERT_TRUE(st.printedMfg);
  TEST_ASSERT_TRUE(st.printedSvc);
}

void test_decide_new_device_no_fields_still_reports() {
  DeviceState st;
  PrintDecision p = decide(true, false, false, false, st);
  // A brand-new device is always reported, even with nothing to append.
  TEST_ASSERT_TRUE(p.show);
  TEST_ASSERT_TRUE(p.isNew);
  TEST_ASSERT_FALSE(p.showName);
  TEST_ASSERT_FALSE(p.showMfg);
  TEST_ASSERT_FALSE(p.showSvc);
  // Nothing shown, so no field state is committed.
  TEST_ASSERT_FALSE(st.printedName);
  TEST_ASSERT_FALSE(st.printedMfg);
  TEST_ASSERT_FALSE(st.printedSvc);
}

void test_decide_unchanged_device_not_reported() {
  DeviceState st;
  decide(true, true, true, true, st);  // first sighting, everything shown
  PrintDecision p = decide(false, true, true, true, st);  // same fields again
  TEST_ASSERT_FALSE(p.show);
  TEST_ASSERT_FALSE(p.showName);
  TEST_ASSERT_FALSE(p.showMfg);
  TEST_ASSERT_FALSE(p.showSvc);
}

void test_decide_gains_name_after_first_sighting() {
  DeviceState st;
  decide(true, false, false, false, st);   // new, no fields
  PrintDecision p = decide(false, true, false, false, st);  // later gains a name
  TEST_ASSERT_TRUE(p.show);
  TEST_ASSERT_FALSE(p.isNew);
  TEST_ASSERT_TRUE(p.showName);
  TEST_ASSERT_FALSE(p.showMfg);
  TEST_ASSERT_FALSE(p.showSvc);
  TEST_ASSERT_TRUE(st.printedName);   // committed only now that it was shown
  TEST_ASSERT_FALSE(st.printedMfg);
  TEST_ASSERT_FALSE(st.printedSvc);
}

// --- AddressRegistry ---------------------------------------------------------
void test_registry_is_new_and_unique_count() {
  AddressRegistry reg;
  TEST_ASSERT_TRUE(reg.is_new("aa:bb:cc:dd:ee:ff"));
  TEST_ASSERT_FALSE(reg.is_new("aa:bb:cc:dd:ee:ff"));  // seen before
  TEST_ASSERT_TRUE(reg.is_new("11:22:33:44:55:66"));
  TEST_ASSERT_EQUAL_UINT32(2u, reg.unique_count());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_company_id_short_returns_unknown);
  RUN_TEST(test_company_id_little_endian);
  RUN_TEST(test_company_id_name_known_and_unknown);
  RUN_TEST(test_name_matches_empty_filter_always_true);
  RUN_TEST(test_name_matches_case_insensitive_substring);
  RUN_TEST(test_name_matches_non_match);
  RUN_TEST(test_decide_new_device_all_fields);
  RUN_TEST(test_decide_new_device_no_fields_still_reports);
  RUN_TEST(test_decide_unchanged_device_not_reported);
  RUN_TEST(test_decide_gains_name_after_first_sighting);
  RUN_TEST(test_registry_is_new_and_unique_count);
  return UNITY_END();
}
