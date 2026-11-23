/* Pure software fault injection. Includes production internals solely to verify
 * the actual write_one drain and per-device named mutex. No HID API is called. */
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

static BOOL WINAPI simulated_write(HANDLE, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED);
static BOOL WINAPI simulated_cancel(HANDLE, LPOVERLAPPED);
static BOOL WINAPI simulated_result(HANDLE, LPOVERLAPPED, LPDWORD, BOOL);
static DWORD WINAPI simulated_wait(HANDLE, DWORD);
#define WriteFile simulated_write
#define CancelIoEx simulated_cancel
#define GetOverlappedResult simulated_result
#define WaitForSingleObject simulated_wait
#include "../src/bridge.c"
#undef WriteFile
#undef CancelIoEx
#undef GetOverlappedResult
#undef WaitForSingleObject

static HANDLE fake_device = (HANDLE)(UINT_PTR)0x1234;
static volatile LONG mode, result_calls, cancel_calls, pending_checks;
static HANDLE drain_release, cancelled_event;

static BOOL WINAPI simulated_write(HANDLE device, LPCVOID data, DWORD size, LPDWORD bytes, LPOVERLAPPED ov)
{
    assert(device == fake_device && data == write_report && ov == &write_ov);
    assert(size == 65 && write_report[0] == 0 && write_report[1] == 0 && write_report[2] == 40);
    *bytes = 0;
    if (mode == 0) { *bytes = 65; return TRUE; }
    if (mode == 3) { *bytes = 64; return TRUE; }
    SetLastError(ERROR_IO_PENDING); return FALSE;
}
static BOOL WINAPI simulated_cancel(HANDLE device, LPOVERLAPPED ov)
{
    assert(device == fake_device && ov == &write_ov);
    InterlockedIncrement(&cancel_calls);
    if (cancelled_event) SetEvent(cancelled_event);
    SetLastError(ERROR_NOT_FOUND); /* Cancellation race must still drain. */
    return FALSE;
}
static BOOL WINAPI simulated_result(HANDLE device, LPOVERLAPPED ov, LPDWORD bytes, BOOL wait)
{
    LONG n = InterlockedIncrement(&result_calls);
    assert(device == fake_device && ov == &write_ov && !wait);
    assert(ov->hEvent && write_report[2] == 40);
    if ((mode == 1 || mode == 4) && n > 3) { SetLastError(ERROR_OPERATION_ABORTED); return FALSE; }
    if (mode == 2 && WaitForSingleObject(drain_release, 0) == WAIT_OBJECT_0) { *bytes = 65; return TRUE; }
    InterlockedIncrement(&pending_checks);
    SetLastError(ERROR_IO_INCOMPLETE); return FALSE;
}
static DWORD WINAPI simulated_wait(HANDLE handle, DWORD timeout)
{
    if (handle == write_ov.hEvent) {
        assert(timeout == 100);
        if (mode == 2) WaitForSingleObject(drain_release, 5);
        if (mode == 4) return WAIT_OBJECT_0; /* spurious signal, still pending */
        return WAIT_TIMEOUT;
    }
    return WaitForSingleObject(handle, timeout);
}
static DWORD WINAPI drain_thread(LPVOID unused)
{
    (void)unused;
    assert(write_one(fake_device) == 0);
    return 0;
}
static DWORD WINAPI contend_thread(LPVOID arg)
{
    HANDLE claimed = claim_device((const wchar_t *)arg);
    if (claimed) { ReleaseMutex(claimed); CloseHandle(claimed); return 1; }
    return 0;
}
static void assert_contend(const wchar_t *path, DWORD expected)
{
    DWORD code = 99;
    HANDLE thread = CreateThread(NULL, 0, contend_thread, (LPVOID)path, 0, NULL);
    assert(thread && WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    assert(GetExitCodeThread(thread, &code) && code == expected);
    CloseHandle(thread);
}
int main(void)
{
    HANDLE owner;
    uint8_t rgb[18] = {0};
    write_ov.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL); assert(write_ov.hEvent);
    assert(bridge_encode_led(1, rgb, 40, write_report, 65) == BRIDGE_OK);
    mode = 0; assert(write_one(fake_device) == 1);
    mode = 3; assert(write_one(fake_device) == 0); /* short write is failure */
    mode = 1; result_calls = 0; cancel_calls = 0; pending_checks = 0;
    assert(write_one(fake_device) == 0);
    assert(cancel_calls == 1 && result_calls == 4 && pending_checks == 3);
    mode = 4; result_calls = 0; cancel_calls = 0; pending_checks = 0;
    assert(write_one(fake_device) == 0);
    assert(cancel_calls == 1 && result_calls == 4 && pending_checks == 3);
    /* A real test thread stays alive while simulated cancellation is pending.
     * Shutdown times out without freeing it, then joins once completion arrives. */
    drain_release = CreateEventW(NULL, TRUE, FALSE, NULL);
    cancelled_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    assert(drain_release && cancelled_event);
    mode = 2; result_calls = 0; cancel_calls = 0;
    worker_thread = CreateThread(NULL, 0, drain_thread, NULL, 0, NULL); assert(worker_thread);
    assert(WaitForSingleObject(cancelled_event, 1000) == WAIT_OBJECT_0);
    assert(bridge_shutdown(10) == BRIDGE_WAIT_TIMEOUT);
    assert(WaitForSingleObject(worker_thread, 0) == WAIT_TIMEOUT);
    assert(write_ov.hEvent && write_report[2] == 40 && cancel_calls == 1);
    SetEvent(drain_release);
    assert(bridge_shutdown(1000) == BRIDGE_OK);
    assert(cancel_calls == 1);
    CloseHandle(worker_thread); worker_thread = NULL;
    CloseHandle(write_ov.hEvent); write_ov.hEvent = NULL;
    CloseHandle(drain_release); CloseHandle(cancelled_event);
    owner = claim_device(L"\\\\?\\hid#testpath1"); assert(owner);
    assert_contend(L"\\\\?\\HID#TESTPATH1", 0); /* same normalized device */
    assert_contend(L"\\\\?\\hid#testpath2", 1); /* different device */
    ReleaseMutex(owner); CloseHandle(owner);
    assert_contend(L"\\\\?\\hid#testpath1", 1);
    puts("PASS lifetime: pending timeout/cancel/drain, short write, shutdown timeout/join, named mutex contention");
    return 0;
}
