#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <wchar.h>
#include "../src/bridge.h"

static HMODULE bridge;
#define LOAD(ret, name, args) \
    typedef ret (__cdecl *name##_type) args; \
    name##_type name; \
    do { FARPROC raw = GetProcAddress(bridge, #name); assert(raw); memcpy(&name, &raw, sizeof(name)); } while (0)

int wmain(int argc, wchar_t **argv)
{
    uint8_t a = 0, b = 0, id[12] = {0};
    uint64_t wide = 0;
    uint16_t code = 0;
    int16_t lever = 0;
    int missing;
    assert(argc == 3);
    missing = wcscmp(argv[2], L"missing") == 0;
    /* argv[1] is always an absolute test sandbox DLL path. */
    bridge = LoadLibraryW(argv[1]); assert(bridge);
    LOAD(uint16_t, mu3_io_get_api_version, (void));
    LOAD(uint32_t, mu3_io_init, (void));
    LOAD(uint32_t, mu3_io_poll, (void));
    LOAD(void, mu3_io_get_opbtns, (uint8_t *));
    LOAD(void, mu3_io_get_gamebtns, (uint8_t *, uint8_t *));
    LOAD(void, mu3_io_get_lever, (int16_t *));
    LOAD(uint32_t, mu3_io_led_init, (void));
    LOAD(void, mu3_io_led_set_colors, (uint8_t, const uint8_t *));
    LOAD(uint8_t, mu3_io_is_busy, (void));
    LOAD(uint8_t, mu3_io_is_connected, (int32_t));
    LOAD(uint16_t, aime_io_get_api_version, (void));
    LOAD(uint32_t, aime_io_init, (void));
    LOAD(uint32_t, aime_io_nfc_poll, (uint8_t));
    LOAD(uint32_t, aime_io_nfc_get_felica_id, (uint8_t, uint64_t *));
    LOAD(uint32_t, aime_io_nfc_get_felica_pm, (uint8_t, uint64_t *));
    LOAD(uint32_t, aime_io_nfc_get_felica_system_code, (uint8_t, uint16_t *));
    LOAD(uint32_t, aime_io_nfc_get_aime_id, (uint8_t, uint8_t *, uint64_t));
    LOAD(void, aime_io_led_set_color, (uint8_t, uint8_t, uint8_t, uint8_t));
    LOAD(uint32_t, bridge_shutdown, (uint32_t));
    LOAD(uint32_t, bridge_encode_led, (uint8_t, const uint8_t *, uint8_t, uint8_t *, uint64_t));
    assert(aime_io_nfc_get_felica_id(0, NULL) == BRIDGE_E_INVALIDARG);
    assert(aime_io_nfc_get_felica_pm(0, NULL) == BRIDGE_E_INVALIDARG);
    assert(aime_io_nfc_get_felica_system_code(0, NULL) == BRIDGE_E_INVALIDARG);
    assert(aime_io_nfc_get_aime_id(0, NULL, 10) == BRIDGE_E_INVALIDARG);
    assert(aime_io_nfc_get_aime_id(0, id, 9) == BRIDGE_E_INVALIDARG);
    mu3_io_get_opbtns(NULL); mu3_io_get_gamebtns(NULL, NULL); mu3_io_get_lever(NULL);
    assert(mu3_io_get_api_version() == (missing ? 0 : 0x2345));
    assert(aime_io_get_api_version() == (missing ? 0 : 0x4567));
    assert(mu3_io_init() == (missing ? BRIDGE_E_FAIL : UINT32_C(0x81234567)));
    assert(mu3_io_poll() == (missing ? BRIDGE_E_FAIL : UINT32_C(0x87654321)));
    assert(mu3_io_led_init() == (missing ? BRIDGE_E_FAIL : UINT32_C(0x80070005)));
    mu3_io_get_opbtns(&a); assert(a == (missing ? 0 : 0xa5));
    mu3_io_get_gamebtns(&a, &b); assert(a == (missing ? 0 : 0x81) && b == (missing ? 0 : 0x42));
    mu3_io_get_gamebtns(&a, NULL); assert(a == (missing ? 0 : 0x81));
    mu3_io_get_gamebtns(NULL, &b); assert(b == (missing ? 0 : 0x42));
    mu3_io_get_lever(&lever); assert(lever == (missing ? 0 : -12345));
    assert(mu3_io_is_busy() == (missing ? 0 : 0x81));
    assert(mu3_io_is_connected(-1234567) == (missing ? 0 : 0x42));
    assert(aime_io_init() == (missing ? BRIDGE_E_FAIL : UINT32_C(0x82345678)));
    assert(aime_io_nfc_poll(0xe1) == (missing ? BRIDGE_E_FAIL : UINT32_C(0x83456789)));
    assert(aime_io_nfc_get_felica_id(0xe2, &wide) == (missing ? BRIDGE_E_FAIL : UINT32_C(0x8456789a)));
    assert(wide == (missing ? 0 : UINT64_C(0xfedcba9876543210)));
    assert(aime_io_nfc_get_felica_pm(0xe3, &wide) == (missing ? BRIDGE_E_FAIL : UINT32_C(0x856789ab)));
    assert(wide == (missing ? 0 : UINT64_C(0x876543210fedcba9)));
    assert(aime_io_nfc_get_felica_system_code(0xe4, &code) == (missing ? BRIDGE_E_FAIL : UINT32_C(0x86789abc)));
    assert(code == (missing ? 0 : 0xfedc));
    assert(aime_io_nfc_get_aime_id(0xe5, id, UINT64_C(0x100000007)) == (missing ? BRIDGE_E_FAIL : UINT32_C(0x8789abcd)));
    if (!missing) for (int i = 0; i < 10; ++i) assert(id[i] == (uint8_t)(0xf0 + i));
    assert(id[10] == 0 && id[11] == 0);
    aime_io_led_set_color(0xe6, 0xf1, 0xf2, 0xf3);
    /* enabled=0 is written by the test script before launch. NO HID APIs run. */
    { uint8_t rgb[18] = {0}, report[65];
      assert(bridge_encode_led(1, rgb, 40, report, 65) == BRIDGE_OK);
      mu3_io_led_set_colors(1, rgb); mu3_io_led_set_colors(1, NULL); mu3_io_led_set_colors(0, rgb);
    }
    assert(bridge_shutdown(1000) == BRIDGE_OK);
    assert(bridge_shutdown(0) == BRIDGE_OK);
    if (!missing) {
        HMODULE fake = GetModuleHandleW(L"MU3Input.dll");
        uint32_t (__cdecl *count)(void); FARPROC raw;
        assert(fake); raw = GetProcAddress(fake, "fake_calls"); assert(raw); memcpy(&count, &raw, sizeof(count));
        assert(count() == 19); /* all 17 forwarded symbols, get_gamebtns x3 */
    }
    puts(missing ? "PASS missing: no CWD fallback, version0/E_FAIL, safe outputs/nulls" : "PASS forward: all 18 APIs, exact x64 widths, uint64 size >4GB, no LED forward");
    /* No manual unload; bridge is pinned once first API initializes config. */
    return 0;
}
