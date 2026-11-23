#include "../src/bridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t rgb[18], report[70], expected[65] = {0}, alias[65];
    unsigned i, b;
    for (i = 0; i < 18; ++i) rgb[i] = (uint8_t)(i + 1);
    expected[2] = 40;
    memcpy(expected + 3, rgb, 9);
    memcpy(expected + 18, rgb + 9, 9);
    memset(report, 0xa5, sizeof(report));
    assert(bridge_encode_led(1, rgb, 40, report, 65) == BRIDGE_OK);
    assert(memcmp(report, expected, 65) == 0);
    for (i = 65; i < sizeof(report); ++i) assert(report[i] == 0xa5);
    for (b = 0; b < 256; ++b) {
        assert(bridge_encode_led(1, rgb, (uint8_t)b, report, 65) == BRIDGE_OK);
        assert(report[0] == 0 && report[1] == 0);
        assert(report[2] == (b > 40 ? 40 : b));
        for (i = 0; i < 65; ++i) if (i != 2) assert(report[i] == expected[i]);
    }
    memcpy(alias, rgb, 18);
    assert(bridge_encode_led(1, alias, 40, alias, 65) == BRIDGE_OK);
    assert(memcmp(alias, expected, 65) == 0);
    memset(report, 0xa5, sizeof(report));
    assert(bridge_encode_led(0, rgb, 40, report, 65) == BRIDGE_E_INVALIDARG);
    assert(bridge_encode_led(2, rgb, 40, report, 65) == BRIDGE_E_INVALIDARG);
    assert(bridge_encode_led(1, NULL, 40, report, 65) == BRIDGE_E_INVALIDARG);
    assert(bridge_encode_led(1, rgb, 40, NULL, 65) == BRIDGE_E_INVALIDARG);
    assert(bridge_encode_led(1, rgb, 40, report, 64) == BRIDGE_E_INVALIDARG);
    for (i = 0; i < sizeof(report); ++i) assert(report[i] == 0xa5);
    puts("PASS encode: exact 65B, all 256 brightnesses, slots, zero padding, alias/null/size guards");
    return 0;
}
