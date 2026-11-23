#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "bridge.h"

_Static_assert(sizeof(void *) == 8, "x64 build required");
_Static_assert(sizeof(uint32_t) == 4 && sizeof(uint64_t) == 8, "ABI widths");

static HMODULE self_module;
static INIT_ONCE config_once = INIT_ONCE_STATIC_INIT;
static INIT_ONCE original_once = INIT_ONCE_STATIC_INIT;
static wchar_t original_path[32768];
static int led_enabled = 1;
static uint8_t brightness = 40;
static HMODULE original_module; /* Retained for process lifetime. */

#define FORWARD_LIST(X) \
 X(uint16_t, mu3_io_get_api_version, (void)) \
 X(uint32_t, mu3_io_init, (void)) \
 X(uint32_t, mu3_io_poll, (void)) \
 X(void, mu3_io_get_opbtns, (uint8_t *)) \
 X(void, mu3_io_get_gamebtns, (uint8_t *, uint8_t *)) \
 X(void, mu3_io_get_lever, (int16_t *)) \
 X(uint32_t, mu3_io_led_init, (void)) \
 X(uint8_t, mu3_io_is_busy, (void)) \
 X(uint8_t, mu3_io_is_connected, (int32_t)) \
 X(uint16_t, aime_io_get_api_version, (void)) \
 X(uint32_t, aime_io_init, (void)) \
 X(uint32_t, aime_io_nfc_poll, (uint8_t)) \
 X(uint32_t, aime_io_nfc_get_felica_id, (uint8_t, uint64_t *)) \
 X(uint32_t, aime_io_nfc_get_felica_pm, (uint8_t, uint64_t *)) \
 X(uint32_t, aime_io_nfc_get_felica_system_code, (uint8_t, uint16_t *)) \
 X(uint32_t, aime_io_nfc_get_aime_id, (uint8_t, uint8_t *, uint64_t)) \
 X(void, aime_io_led_set_color, (uint8_t, uint8_t, uint8_t, uint8_t))
#define DECLARE_FORWARD(ret, name, params) typedef ret (__cdecl *name##_fn) params; static name##_fn p_##name;
FORWARD_LIST(DECLARE_FORWARD)
#undef DECLARE_FORWARD

static BOOL CALLBACK initialize_config(PINIT_ONCE once, PVOID arg, PVOID *ctx)
{
    wchar_t ini[32768];
    DWORD length;
    wchar_t *slash;
    HMODULE pinned = NULL;
    UINT value;
    (void)once; (void)arg; (void)ctx;
    length = GetModuleFileNameW(self_module, ini, (DWORD)(sizeof(ini) / sizeof(ini[0])));
    if (!length || length >= sizeof(ini) / sizeof(ini[0])) return TRUE;
    /* Pin outside DllMain. FreeLibrary/manual unload is deliberately unsupported. */
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, ini, &pinned)) return TRUE;
    slash = wcsrchr(ini, L'\\');
    if (!slash) return TRUE;
    slash[1] = L'\0';
    if (wcslen(ini) + 30 >= sizeof(ini) / sizeof(ini[0])) return TRUE;
    wcscpy(original_path, ini);
    wcscat(original_path, L"MU3Input.dll");
    wcscat(ini, L"MU3Input.LegacyBridge.ini");
    led_enabled = GetPrivateProfileIntW(L"bridge", L"enabled", 1, ini) != 0;
    value = GetPrivateProfileIntW(L"bridge", L"brightness", 40, ini);
    brightness = (uint8_t)(value > 40 ? 40 : value);
    return TRUE;
}

static BOOL CALLBACK initialize_original(PINIT_ONCE once, PVOID arg, PVOID *ctx)
{
    (void)once; (void)arg; (void)ctx;
    InitOnceExecuteOnce(&config_once, initialize_config, NULL, NULL);
    if (!original_path[0]) return TRUE;
    /* Absolute sibling only. Never CWD, PATH, or recursive bridge lookup. */
    original_module = LoadLibraryExW(original_path, NULL,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!original_module || original_module == self_module) return TRUE;
#define LOAD_FORWARD(ret, name, params) \
    do { FARPROC proc = GetProcAddress(original_module, #name); \
         _Static_assert(sizeof(proc) == sizeof(p_##name), "function pointer width"); \
         memcpy(&p_##name, &proc, sizeof(proc)); } while (0);
    FORWARD_LIST(LOAD_FORWARD)
#undef LOAD_FORWARD
    return TRUE;
}
static void ensure_original(void)
{
    InitOnceExecuteOnce(&original_once, initialize_original, NULL, NULL);
}

uint16_t __cdecl mu3_io_get_api_version(void) { ensure_original(); return p_mu3_io_get_api_version ? p_mu3_io_get_api_version() : 0; }
uint32_t __cdecl mu3_io_init(void) { ensure_original(); return p_mu3_io_init ? p_mu3_io_init() : BRIDGE_E_FAIL; }
uint32_t __cdecl mu3_io_poll(void) { ensure_original(); return p_mu3_io_poll ? p_mu3_io_poll() : BRIDGE_E_FAIL; }
void __cdecl mu3_io_get_opbtns(uint8_t *v) { if (!v) return; *v = 0; ensure_original(); if (p_mu3_io_get_opbtns) p_mu3_io_get_opbtns(v); }
void __cdecl mu3_io_get_gamebtns(uint8_t *l, uint8_t *r)
{
    uint8_t dummy_l = 0, dummy_r = 0;
    if (l) *l = 0;
    if (r) *r = 0;
    if (!l && !r) return;
    ensure_original();
    if (p_mu3_io_get_gamebtns) p_mu3_io_get_gamebtns(l ? l : &dummy_l, r ? r : &dummy_r);
}
void __cdecl mu3_io_get_lever(int16_t *v) { if (!v) return; *v = 0; ensure_original(); if (p_mu3_io_get_lever) p_mu3_io_get_lever(v); }
uint32_t __cdecl mu3_io_led_init(void) { ensure_original(); return p_mu3_io_led_init ? p_mu3_io_led_init() : BRIDGE_E_FAIL; }
uint8_t __cdecl mu3_io_is_busy(void) { ensure_original(); return p_mu3_io_is_busy ? p_mu3_io_is_busy() : 0; }
uint8_t __cdecl mu3_io_is_connected(int32_t n) { ensure_original(); return p_mu3_io_is_connected ? p_mu3_io_is_connected(n) : 0; }
uint16_t __cdecl aime_io_get_api_version(void) { ensure_original(); return p_aime_io_get_api_version ? p_aime_io_get_api_version() : 0; }
uint32_t __cdecl aime_io_init(void) { ensure_original(); return p_aime_io_init ? p_aime_io_init() : BRIDGE_E_FAIL; }
uint32_t __cdecl aime_io_nfc_poll(uint8_t n) { ensure_original(); return p_aime_io_nfc_poll ? p_aime_io_nfc_poll(n) : BRIDGE_E_FAIL; }
uint32_t __cdecl aime_io_nfc_get_felica_id(uint8_t n, uint64_t *v) { if (!v) return BRIDGE_E_INVALIDARG; *v = 0; ensure_original(); return p_aime_io_nfc_get_felica_id ? p_aime_io_nfc_get_felica_id(n, v) : BRIDGE_E_FAIL; }
uint32_t __cdecl aime_io_nfc_get_felica_pm(uint8_t n, uint64_t *v) { if (!v) return BRIDGE_E_INVALIDARG; *v = 0; ensure_original(); return p_aime_io_nfc_get_felica_pm ? p_aime_io_nfc_get_felica_pm(n, v) : BRIDGE_E_FAIL; }
uint32_t __cdecl aime_io_nfc_get_felica_system_code(uint8_t n, uint16_t *v) { if (!v) return BRIDGE_E_INVALIDARG; *v = 0; ensure_original(); return p_aime_io_nfc_get_felica_system_code ? p_aime_io_nfc_get_felica_system_code(n, v) : BRIDGE_E_FAIL; }
uint32_t __cdecl aime_io_nfc_get_aime_id(uint8_t n, uint8_t *v, uint64_t size)
{
    if (!v || size < 10) return BRIDGE_E_INVALIDARG;
    ensure_original();
    return p_aime_io_nfc_get_aime_id ? p_aime_io_nfc_get_aime_id(n, v, size) : BRIDGE_E_FAIL;
}
void __cdecl aime_io_led_set_color(uint8_t n, uint8_t r, uint8_t g, uint8_t b) { ensure_original(); if (p_aime_io_led_set_color) p_aime_io_led_set_color(n, r, g, b); }

/* No resources here are destroyed while the process lives. One writer owns all
 * device I/O. A timed-out cancellation can leave it draining forever, safely. */
static SRWLOCK worker_lock = SRWLOCK_INIT;
static HANDLE worker_thread, wake_event;
static volatile LONG stopping;
static uint8_t latest_report[BRIDGE_REPORT_BYTES];
static int have_latest;
static OVERLAPPED write_ov;
static uint8_t write_report[BRIDGE_REPORT_BYTES];

static int no_report_ids(PHIDP_PREPARSED_DATA pp, const HIDP_CAPS *caps)
{
    HIDP_VALUE_CAPS *values = NULL;
    HIDP_BUTTON_CAPS *buttons = NULL;
    USHORT count, i;
    int ok = 1;
    /* Validate both directions, not merely the total byte lengths. */
    for (int direction = 0; direction != 2; ++direction) {
        HIDP_REPORT_TYPE type = direction ? HidP_Output : HidP_Input;
        count = direction ? caps->NumberOutputValueCaps : caps->NumberInputValueCaps;
        if (count) {
            values = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)count * sizeof(*values));
            if (!values || HidP_GetValueCaps(type, values, &count, pp) != HIDP_STATUS_SUCCESS) { ok = 0; break; }
            for (i = 0; i < count; ++i) if (values[i].ReportID != 0) ok = 0;
            HeapFree(GetProcessHeap(), 0, values); values = NULL;
        }
        count = direction ? caps->NumberOutputButtonCaps : caps->NumberInputButtonCaps;
        if (count) {
            buttons = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)count * sizeof(*buttons));
            if (!buttons || HidP_GetButtonCaps(type, buttons, &count, pp) != HIDP_STATUS_SUCCESS) { ok = 0; break; }
            for (i = 0; i < count; ++i) if (buttons[i].ReportID != 0) ok = 0;
            HeapFree(GetProcessHeap(), 0, buttons); buttons = NULL;
        }
    }
    if (values) HeapFree(GetProcessHeap(), 0, values);
    if (buttons) HeapFree(GetProcessHeap(), 0, buttons);
    return ok;
}

static int matches_device(HANDLE h)
{
    HIDD_ATTRIBUTES attr;
    PHIDP_PREPARSED_DATA pp = NULL;
    HIDP_CAPS caps;
    int ok = 0;
    memset(&attr, 0, sizeof(attr)); attr.Size = sizeof(attr);
    if (!HidD_GetAttributes(h, &attr) || attr.VendorID != 0x2341 || attr.ProductID != 0x8036) return 0;
    if (HidD_GetPreparsedData(h, &pp)) {
        if (HidP_GetCaps(pp, &caps) == HIDP_STATUS_SUCCESS &&
            caps.UsagePage == 0xffc0 && caps.Usage == 0x0c00 &&
            caps.InputReportByteLength == 65 && caps.OutputReportByteLength == 65)
            ok = no_report_ids(pp, &caps);
        HidD_FreePreparsedData(pp);
    }
    return ok;
}

/* Mutex is acquired/released by this same writer thread. Local namespace matches
 * the game/amdaemon logon session. Hash case-insensitive device path (FNV-1a). */
static HANDLE claim_device(const wchar_t *path)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    wchar_t name[80];
    HANDLE mutex;
    DWORD wait;
    for (; *path; ++path) {
        wchar_t c = *path;
        if (c >= L'A' && c <= L'Z') c += L'a' - L'A';
        hash ^= (uint16_t)c;
        hash *= UINT64_C(1099511628211);
    }
    swprintf(name, sizeof(name) / sizeof(name[0]), L"Local\\MU3LegacyLED_%016llx", (unsigned long long)hash);
    mutex = CreateMutexW(NULL, FALSE, name);
    if (!mutex) return NULL;
    wait = WaitForSingleObject(mutex, 0);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED) return mutex;
    CloseHandle(mutex);
    return NULL;
}

static HANDLE open_device(HANDLE *ownership)
{
    GUID guid;
    HDEVINFO devices;
    SP_DEVICE_INTERFACE_DATA iface;
    HANDLE result = INVALID_HANDLE_VALUE;
    DWORD index;
    *ownership = NULL;
    HidD_GetHidGuid(&guid);
    devices = SetupDiGetClassDevsW(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devices == INVALID_HANDLE_VALUE) return result;
    for (index = 0; !InterlockedCompareExchange(&stopping, 0, 0); ++index) {
        DWORD needed = 0;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail;
        HANDLE probe, owner;
        memset(&iface, 0, sizeof(iface)); iface.cbSize = sizeof(iface);
        if (!SetupDiEnumDeviceInterfaces(devices, NULL, &guid, index, &iface)) break;
        SetupDiGetDeviceInterfaceDetailW(devices, &iface, NULL, 0, &needed, NULL);
        if (needed < sizeof(*detail)) continue;
        detail = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, needed);
        if (!detail) break;
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(devices, &iface, detail, needed, NULL, NULL)) { HeapFree(GetProcessHeap(), 0, detail); continue; }
        probe = CreateFileW(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
        if (probe == INVALID_HANDLE_VALUE) { HeapFree(GetProcessHeap(), 0, detail); continue; }
        if (!matches_device(probe)) { CloseHandle(probe); HeapFree(GetProcessHeap(), 0, detail); continue; }
        CloseHandle(probe);
        owner = claim_device(detail->DevicePath);
        if (owner) {
            result = CreateFileW(detail->DevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
            if (result != INVALID_HANDLE_VALUE && !matches_device(result)) { CloseHandle(result); result = INVALID_HANDLE_VALUE; }
            if (result == INVALID_HANDLE_VALUE) { ReleaseMutex(owner); CloseHandle(owner); }
            else *ownership = owner;
        }
        HeapFree(GetProcessHeap(), 0, detail);
        if (result != INVALID_HANDLE_VALUE) break;
    }
    SetupDiDestroyDeviceInfoList(devices);
    return result;
}

static int write_one(HANDLE device)
{
    DWORD bytes = 0, error;
    BOOL ok;
    ResetEvent(write_ov.hEvent);
    write_ov.Internal = 0; write_ov.InternalHigh = 0;
    write_ov.Offset = 0; write_ov.OffsetHigh = 0;
    ok = WriteFile(device, write_report, sizeof(write_report), &bytes, &write_ov);
    if (ok) return bytes == sizeof(write_report);
    error = GetLastError();
    if (error != ERROR_IO_PENDING) return 0;
    if (WaitForSingleObject(write_ov.hEvent, 100) == WAIT_OBJECT_0) {
        ok = GetOverlappedResult(device, &write_ov, &bytes, FALSE);
        if (ok) return bytes == sizeof(write_report);
        if (GetLastError() != ERROR_IO_INCOMPLETE) return 0;
        /* A spurious/stale event is not proof the request completed. */
    }
    /* Timeout/failure: cancel only our request, then drain it. Never recycle
     * buffer, OVERLAPPED, event, or device while ERROR_IO_INCOMPLETE persists. */
    CancelIoEx(device, &write_ov);
    for (;;) {
        ok = GetOverlappedResult(device, &write_ov, &bytes, FALSE);
        if (ok || GetLastError() != ERROR_IO_INCOMPLETE) break;
        WaitForSingleObject(write_ov.hEvent, 100);
    }
    return 0; /* Even a cancellation race success reconnects conservatively. */
}

static DWORD WINAPI writer_main(LPVOID unused)
{
    HANDLE device = INVALID_HANDLE_VALUE, owner = NULL;
    uint8_t last[BRIDGE_REPORT_BYTES];
    int have_last = 0;
    ULONGLONG next_write = 0, retry_at = 0, idle_until = 0;
    (void)unused;
    memset(&write_ov, 0, sizeof(write_ov));
    write_ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!write_ov.hEvent) return 1;
    while (!InterlockedCompareExchange(&stopping, 0, 0)) {
        int available;
        ULONGLONG now;
        AcquireSRWLockShared(&worker_lock);
        available = have_latest;
        if (available) memcpy(write_report, latest_report, sizeof(write_report));
        ReleaseSRWLockShared(&worker_lock);
        if (!available) { WaitForSingleObject(wake_event, INFINITE); continue; }
        now = GetTickCount64();
        if (now < retry_at) { WaitForSingleObject(wake_event, (DWORD)(retry_at - now)); continue; }
        if (device == INVALID_HANDLE_VALUE) {
            device = open_device(&owner);
            if (device == INVALID_HANDLE_VALUE) { retry_at = GetTickCount64() + 1000; continue; }
            have_last = 0;
        }
        if (have_last && memcmp(last, write_report, sizeof(last)) == 0) {
            now = GetTickCount64();
            if (now < idle_until) {
                WaitForSingleObject(wake_event, (DWORD)(idle_until - now));
                continue;
            }
            /* Static colors do not imply a live device. Release our idle lease
             * after 500ms, then yield a full second to another process. The next
             * handle generation resets dedupe and replays latest even unchanged,
             * recovering same-color unplug/replug without polling device reads. */
            CloseHandle(device); device = INVALID_HANDLE_VALUE;
            ReleaseMutex(owner); CloseHandle(owner); owner = NULL;
            retry_at = now + 1000;
            continue;
        }
        now = GetTickCount64();
        if (now < next_write) { WaitForSingleObject(wake_event, (DWORD)(next_write - now)); continue; }
        if (InterlockedCompareExchange(&stopping, 0, 0)) break;
        if (write_one(device)) {
            memcpy(last, write_report, sizeof(last)); have_last = 1;
            idle_until = GetTickCount64() + 500;
        }
        else {
            CloseHandle(device); device = INVALID_HANDLE_VALUE;
            ReleaseMutex(owner); CloseHandle(owner); owner = NULL;
            have_last = 0; retry_at = GetTickCount64() + 1000;
        }
        next_write = GetTickCount64() + 34; /* <= 29.412Hz, never exceeds 30Hz */
    }
    if (device != INVALID_HANDLE_VALUE) CloseHandle(device);
    if (owner) { ReleaseMutex(owner); CloseHandle(owner); }
    /* Deliberately retained events/thread handles, no unload cleanup. */
    return 0;
}

/* Caller holds worker_lock exclusively and has checked !stopping. */
static void start_worker_locked(void)
{
    /* A failed event allocation exits writer_main before any I/O. Reap only a
     * proven terminated thread; pending/running workers can never be replaced. */
    if (worker_thread && WaitForSingleObject(worker_thread, 0) == WAIT_OBJECT_0) {
        CloseHandle(worker_thread); worker_thread = NULL;
    }
    if (!wake_event) wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (wake_event && !worker_thread) worker_thread = CreateThread(NULL, 0, writer_main, NULL, 0, NULL);
    if (wake_event) SetEvent(wake_event);
}

void __cdecl mu3_io_led_set_colors(uint8_t board, const uint8_t *rgb)
{
    uint8_t report[BRIDGE_REPORT_BYTES];
    if (board != 1 || !rgb || InterlockedCompareExchange(&stopping, 0, 0)) return;
    InitOnceExecuteOnce(&config_once, initialize_config, NULL, NULL);
    if (!led_enabled || !original_path[0]) return;
    if (bridge_encode_led(board, rgb, brightness, report, sizeof(report)) != BRIDGE_OK) return;
    AcquireSRWLockExclusive(&worker_lock);
    if (!InterlockedCompareExchange(&stopping, 0, 0)) {
        /* Immediate copy: caller stack/payload may disappear on return. */
        memcpy(latest_report, report, sizeof(report)); have_latest = 1;
        start_worker_locked();
    }
    ReleaseSRWLockExclusive(&worker_lock);
}

uint32_t bridge_shutdown(uint32_t timeout_ms)
{
    HANDLE thread;
    DWORD wait;
    InterlockedExchange(&stopping, 1);
    AcquireSRWLockShared(&worker_lock);
    thread = worker_thread;
    if (wake_event) SetEvent(wake_event);
    ReleaseSRWLockShared(&worker_lock);
    if (!thread) return BRIDGE_OK;
    /* 0xffffffff is INFINITE; use finite timeout when a faulty driver is possible. */
    wait = WaitForSingleObject(thread, timeout_ms);
    if (wait == WAIT_OBJECT_0) return BRIDGE_OK;
    return wait == WAIT_TIMEOUT ? BRIDGE_WAIT_TIMEOUT : BRIDGE_E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = instance;
        DisableThreadLibraryCalls(instance);
    }
    /* Never load a library, start a worker, wait, or free pending I/O here. */
    return TRUE;
}
