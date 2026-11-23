#include "bridge.h"
#include <string.h>

uint32_t bridge_encode_led(uint8_t board, const uint8_t *rgb,
                          uint8_t brightness, uint8_t *report,
                          uint64_t report_size)
{
    uint8_t copy[BRIDGE_RGB_BYTES];
    if (!report || !rgb || report_size < BRIDGE_REPORT_BYTES || board != 1)
        return BRIDGE_E_INVALIDARG;
    /* Allow input/output aliasing. Exactly six RGBs, no gamma/reordering. */
    memcpy(copy, rgb, sizeof(copy));
    memset(report, 0, BRIDGE_REPORT_BYTES);
    report[0] = 0; /* Windows no-report-ID prefix, not firmware payload. */
    report[1] = 0; /* Firmware type 0 only; type 1 would write card data. */
    report[2] = brightness > 40 ? 40 : brightness;
    memcpy(report + 3, copy, 9);       /* old RGB slots 0,1,2: left */
    memcpy(report + 3 + 5 * 3, copy + 9, 9); /* slots 5,6,7: right */
    return BRIDGE_OK;
}
