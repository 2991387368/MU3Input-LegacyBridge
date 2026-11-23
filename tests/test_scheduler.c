/* Full production writer loop with fake clock/HID metadata/write boundary.
 * No HID enumeration, device opening, device read/write, or original DLL load. */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static void __stdcall fake_guid(LPGUID);
static HDEVINFO WINAPI fake_devices(const GUID *, PCWSTR, HWND, DWORD);
static BOOL WINAPI fake_enum(HDEVINFO, PSP_DEVINFO_DATA, const GUID *, DWORD, PSP_DEVICE_INTERFACE_DATA);
static BOOL WINAPI fake_detail(HDEVINFO, PSP_DEVICE_INTERFACE_DATA, PSP_DEVICE_INTERFACE_DETAIL_DATA_W, DWORD, PDWORD, PSP_DEVINFO_DATA);
static BOOL WINAPI fake_destroy(HDEVINFO);
static HANDLE WINAPI fake_open(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static BOOL WINAPI fake_close(HANDLE);
static BOOLEAN __stdcall fake_attr(HANDLE, PHIDD_ATTRIBUTES);
static BOOLEAN __stdcall fake_pp(HANDLE, PHIDP_PREPARSED_DATA *);
static BOOLEAN __stdcall fake_free_pp(PHIDP_PREPARSED_DATA);
static NTSTATUS __stdcall fake_caps(PHIDP_PREPARSED_DATA, PHIDP_CAPS);
static BOOL WINAPI fake_write(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static ULONGLONG WINAPI fake_ticks(void);
static DWORD WINAPI fake_wait(HANDLE, DWORD);
static HANDLE WINAPI fake_event(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCWSTR);
#define CreateEventW fake_event
#define HidD_GetHidGuid fake_guid
#define SetupDiGetClassDevsW fake_devices
#define SetupDiEnumDeviceInterfaces fake_enum
#define SetupDiGetDeviceInterfaceDetailW fake_detail
#define SetupDiDestroyDeviceInfoList fake_destroy
#define CreateFileW fake_open
#define CloseHandle fake_close
#define HidD_GetAttributes fake_attr
#define HidD_GetPreparsedData fake_pp
#define HidD_FreePreparsedData fake_free_pp
#define HidP_GetCaps fake_caps
#define WriteFile fake_write
#define GetTickCount64 fake_ticks
#define WaitForSingleObject fake_wait
#include "../src/bridge.c"
#undef CreateEventW
#undef HidD_GetHidGuid
#undef SetupDiGetClassDevsW
#undef SetupDiEnumDeviceInterfaces
#undef SetupDiGetDeviceInterfaceDetailW
#undef SetupDiDestroyDeviceInfoList
#undef CreateFileW
#undef CloseHandle
#undef HidD_GetAttributes
#undef HidD_GetPreparsedData
#undef HidD_FreePreparsedData
#undef HidP_GetCaps
#undef WriteFile
#undef GetTickCount64
#undef WaitForSingleObject

static const wchar_t test_path[] = L"\\\\?\\hid#legacy_bridge_scheduler_test";
static HANDLE probe = (HANDLE)(UINT_PTR)0x4001;
static HANDLE transport = (HANDLE)(UINT_PTR)0x4002;
static HDEVINFO devs = (HDEVINFO)(UINT_PTR)0x4003;
static ULONGLONG clock_ms;
static unsigned opens, writes, closes, enumerations, yields, blocked;
static int connected_handle;
static uint8_t first_report[65];
static volatile LONG fail_write_event = 1;

static HANDLE WINAPI fake_event(LPSECURITY_ATTRIBUTES sa, BOOL manual, BOOL initial, LPCWSTR name)
{
    if (manual && InterlockedExchange(&fail_write_event, 0)) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY); return NULL;
    }
    return CreateEventW(sa, manual, initial, name);
}
static void __stdcall fake_guid(LPGUID p) { memset(p, 0, sizeof(*p)); }
static HDEVINFO WINAPI fake_devices(const GUID *g, PCWSTR n, HWND w, DWORD flags)
{
    assert(g && !n && !w && flags == (DIGCF_PRESENT | DIGCF_DEVICEINTERFACE));
    ++enumerations; return devs;
}
static BOOL WINAPI fake_enum(HDEVINFO d, PSP_DEVINFO_DATA info, const GUID *g, DWORD index, PSP_DEVICE_INTERFACE_DATA iface)
{
    assert(d == devs && !info && g && iface);
    /* Unplug immediately after first successful write; replug by t=2000ms. */
    return index == 0 && (writes == 0 || clock_ms >= 2000);
}
static BOOL WINAPI fake_detail(HDEVINFO d, PSP_DEVICE_INTERFACE_DATA iface, PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail, DWORD size, PDWORD needed, PSP_DEVINFO_DATA info)
{
    DWORD bytes = (DWORD)(sizeof(*detail) + sizeof(test_path));
    assert(d == devs && iface && !info);
    if (needed) *needed = bytes;
    if (!detail) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
    assert(size >= bytes && detail->cbSize == sizeof(*detail));
    wcscpy(detail->DevicePath, test_path); return TRUE;
}
static BOOL WINAPI fake_destroy(HDEVINFO d) { assert(d == devs); return TRUE; }
static HANDLE WINAPI fake_open(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE templ)
{
    assert(wcscmp(path, test_path) == 0 && share == (FILE_SHARE_READ | FILE_SHARE_WRITE));
    assert(!security && creation == OPEN_EXISTING && flags == FILE_FLAG_OVERLAPPED && !templ);
    if (!access) return probe;
    assert(access == GENERIC_WRITE && !connected_handle);
    connected_handle = 1; ++opens; return transport;
}
static BOOL WINAPI fake_close(HANDLE h)
{
    if (h == probe) return TRUE;
    if (h == transport) { assert(connected_handle); connected_handle = 0; ++closes; return TRUE; }
    return CloseHandle(h);
}
static BOOLEAN __stdcall fake_attr(HANDLE h, PHIDD_ATTRIBUTES a)
{
    assert(h == probe || h == transport); a->VendorID = 0x2341; a->ProductID = 0x8036; return TRUE;
}
static BOOLEAN __stdcall fake_pp(HANDLE h, PHIDP_PREPARSED_DATA *p)
{
    assert(h == probe || h == transport); *p = (PHIDP_PREPARSED_DATA)(UINT_PTR)0x4004; return TRUE;
}
static BOOLEAN __stdcall fake_free_pp(PHIDP_PREPARSED_DATA p) { assert(p); return TRUE; }
static NTSTATUS __stdcall fake_caps(PHIDP_PREPARSED_DATA p, PHIDP_CAPS caps)
{
    assert(p); memset(caps, 0, sizeof(*caps)); caps->UsagePage = 0xffc0; caps->Usage = 0x0c00;
    caps->InputReportByteLength = 65; caps->OutputReportByteLength = 65; return HIDP_STATUS_SUCCESS;
}
static BOOL WINAPI fake_write(HANDLE h, LPCVOID data, DWORD size, LPDWORD bytes, LPOVERLAPPED ov)
{
    assert(h == transport && connected_handle && data == write_report && size == 65 && ov == &write_ov);
    ++writes;
    if (writes == 1) { assert(clock_ms == 0); memcpy(first_report, data, 65); }
    else assert(memcmp(first_report, data, 65) == 0); /* unchanged latest MUST replay */
    if (writes == 2) assert(clock_ms == 2500); /* absent at t1500, retry at2500 */
    if (writes == 3) { assert(clock_ms == 4000); InterlockedExchange(&stopping, 1); }
    assert(writes <= 3); *bytes = size; return TRUE;
}
static ULONGLONG WINAPI fake_ticks(void) { return clock_ms; }
static DWORD WINAPI contender(LPVOID unused)
{
    HANDLE owner;
    (void)unused;
    owner = claim_device(test_path);
    if (!owner) return 0;
    ReleaseMutex(owner); CloseHandle(owner); return 1;
}
static DWORD WINAPI fake_wait(HANDLE h, DWORD timeout)
{
    if (h == wake_event) {
        HANDLE thread;
        DWORD claimed = 99;
        assert(timeout != INFINITE && timeout > 0 && timeout <= 1000);
        /* A second writer must be excluded during the 500ms lease and able to
         * acquire the SAME device mutex during the subsequent 1s cooldown. */
        thread = CreateThread(NULL, 0, contender, NULL, 0, NULL);
        assert(thread && WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
        assert(GetExitCodeThread(thread, &claimed)); CloseHandle(thread);
        if (connected_handle) { assert(claimed == 0 && timeout == 500); ++blocked; }
        else { assert(claimed == 1 && timeout == 1000); ++yields; }
        clock_ms += timeout;
        return WAIT_TIMEOUT;
    }
    return WaitForSingleObject(h, timeout);
}
int main(void)
{
    uint8_t rgb[18];
    memset(rgb, 0x12, sizeof(rgb));
    assert(bridge_encode_led(1, rgb, 40, latest_report, 65) == BRIDGE_OK);
    have_latest = 1;
    wake_event = CreateEventW(NULL, FALSE, FALSE, NULL); assert(wake_event);
    AcquireSRWLockExclusive(&worker_lock);
    start_worker_locked();
    ReleaseSRWLockExclusive(&worker_lock);
    assert(worker_thread && WaitForSingleObject(worker_thread, 1000) == WAIT_OBJECT_0);
    { DWORD code = 0; assert(GetExitCodeThread(worker_thread, &code) && code == 1); }
    assert(!write_ov.hEvent && !enumerations && !opens && !writes);
    /* Next producer call reaps the terminated event-failure worker and retries.
     * Use the exact locked helper, avoiding config/original initialization. */
    AcquireSRWLockExclusive(&worker_lock);
    start_worker_locked();
    ReleaseSRWLockExclusive(&worker_lock);
    assert(worker_thread && WaitForSingleObject(worker_thread, 3000) == WAIT_OBJECT_0);
    { DWORD code = 1; assert(GetExitCodeThread(worker_thread, &code) && code == 0); }
    assert(writes == 3 && opens == 3 && closes == 3 && enumerations == 4);
    assert(blocked == 2 && yields == 3 && !connected_handle);
    { HANDLE owner = claim_device(test_path); assert(owner); ReleaseMutex(owner); CloseHandle(owner); }
    CloseHandle(wake_event); CloseHandle(write_ov.hEvent); CloseHandle(worker_thread);
    puts("PASS scheduler: event-allocation retry, 500ms idle release, 1s owner yield, same-report replug replay, dedupe within lease");
    return 0;
}
