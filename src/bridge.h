#ifndef LEGACY_BRIDGE_H
#define LEGACY_BRIDGE_H
#include <stdint.h>

#define BRIDGE_OK UINT32_C(0)
#define BRIDGE_E_FAIL UINT32_C(0x80004005)
#define BRIDGE_E_INVALIDARG UINT32_C(0x80070057)
#define BRIDGE_WAIT_TIMEOUT UINT32_C(258)
#define BRIDGE_REPORT_BYTES 65u
#define BRIDGE_RGB_BYTES 18u

/* x64 Windows ABI. No C bool, unsigned long, or size_t in public contracts. */
uint32_t bridge_encode_led(uint8_t board, const uint8_t *rgb,
                          uint8_t brightness, uint8_t *report,
                          uint64_t report_size);
uint32_t bridge_shutdown(uint32_t timeout_ms);
#endif
