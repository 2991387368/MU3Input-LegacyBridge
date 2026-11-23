/* TEST DOUBLE ONLY. This is not the real MU3Input and performs no I/O. */
#include <stdint.h>
#include <assert.h>
#define API __declspec(dllexport)
static uint32_t calls;
API uint32_t fake_calls(void) { return calls; }
API uint16_t mu3_io_get_api_version(void) { ++calls; return 0x2345; }
API uint32_t mu3_io_init(void) { ++calls; return UINT32_C(0x81234567); }
API uint32_t mu3_io_poll(void) { ++calls; return UINT32_C(0x87654321); }
API void mu3_io_get_opbtns(uint8_t *p) { assert(p); ++calls; *p = 0xa5; }
API void mu3_io_get_gamebtns(uint8_t *l, uint8_t *r) { assert(l && r); ++calls; *l = 0x81; *r = 0x42; }
API void mu3_io_get_lever(int16_t *p) { assert(p); ++calls; *p = -12345; }
API uint32_t mu3_io_led_init(void) { ++calls; return UINT32_C(0x80070005); }
API void mu3_io_led_set_colors(uint8_t board, const uint8_t *p) { (void)board; (void)p; assert(!"LED must NEVER forward"); }
API uint8_t mu3_io_is_busy(void) { ++calls; return 0x81; }
API uint8_t mu3_io_is_connected(int32_t index) { assert(index == -1234567); ++calls; return 0x42; }
API uint16_t aime_io_get_api_version(void) { ++calls; return 0x4567; }
API uint32_t aime_io_init(void) { ++calls; return UINT32_C(0x82345678); }
API uint32_t aime_io_nfc_poll(uint8_t n) { assert(n == 0xe1); ++calls; return UINT32_C(0x83456789); }
API uint32_t aime_io_nfc_get_felica_id(uint8_t n, uint64_t *p) { assert(n == 0xe2 && p); ++calls; *p = UINT64_C(0xfedcba9876543210); return UINT32_C(0x8456789a); }
API uint32_t aime_io_nfc_get_felica_pm(uint8_t n, uint64_t *p) { assert(n == 0xe3 && p); ++calls; *p = UINT64_C(0x876543210fedcba9); return UINT32_C(0x856789ab); }
API uint32_t aime_io_nfc_get_felica_system_code(uint8_t n, uint16_t *p) { assert(n == 0xe4 && p); ++calls; *p = 0xfedc; return UINT32_C(0x86789abc); }
API uint32_t aime_io_nfc_get_aime_id(uint8_t n, uint8_t *p, uint64_t size) { assert(n == 0xe5 && p && size == UINT64_C(0x100000007)); ++calls; for (int i = 0; i < 10; ++i) p[i] = (uint8_t)(0xf0 + i); return UINT32_C(0x8789abcd); }
API void aime_io_led_set_color(uint8_t n, uint8_t r, uint8_t g, uint8_t b) { assert(n == 0xe6 && r == 0xf1 && g == 0xf2 && b == 0xf3); ++calls; }
