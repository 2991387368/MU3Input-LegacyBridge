# MU3Input LegacyBridge

[中文 / main README](../README.md)

A narrowly scoped Windows **x64** native compatibility bridge for legacy Arduino Leonardo ONGEKI controllers and MU3Input 3.1. It forwards input/Aime calls to a separately obtained original DLL, intercepts board-1 RGB18 colors, and writes the legacy ten-slot HID layout using an independent asynchronous writer. No firmware flashing is required.

Not a complete IO implementation, launcher, or general Arduino driver. Not affiliated with or endorsed by upstream projects.

## Compatibility

- Windows x64 host process; not x86 or ARM64.
- Original MU3Input 3.1 exposing the expected 18 functions.
- VID `2341`, PID `8036`, usage page `FFC0`, usage `0C00`.
- No nonzero input/output report IDs; 64-byte application payload, 65-byte Windows HID input/output reports.
- Six RGB lamps in old slots 0–2 and 5–7; only board 1 is supported.

Matching IDs do not prove a different controller is compatible. There is no serial-number selection for multiple matching devices.

## Installation

Exit every host, backend and IO configuration process. Back up existing configuration, then place these files together:

```text
MU3Input.dll                 original 3.1; obtain separately
MU3Input.LegacyBridge.dll     do not rename to MU3Input.dll
MU3Input.LegacyBridge.ini     copy from config/
```

Edit only the relevant sections of the existing loader configuration:

```ini
[mu3io]
path=MU3Input.LegacyBridge.dll
[aimeio]
path=MU3Input.LegacyBridge.dll
```

Both must use the same bridge. It loads the original by absolute sibling path, without CWD/PATH fallback. An old DLL exporting only `mu3_io_set_led(uint)` is not a substitute for the expected 3.1 API.

Keep valid original input JSON in the **host working directory**. The bridge INI is read beside the bridge, once per process:

```ini
[bridge]
enabled=1
brightness=40
```

`enabled=0` disables only bridge LED output; input/Aime forwarding remains active. Brightness is an 8-bit value clamped to 0–40, **not a percentage**. Restart all relevant processes after changing it.

HID `Param` must be an object, not `null`; the main README contains an example array entry. Unneeded UDP backends can cause port-binding conflicts across host processes. `Scope=None` does not prevent backend construction/binding. Invalid or missing original JSON may fall back to default UDP. These are upstream behaviors, not repaired here.

## Build and offline tests

Windows x64, PowerShell and MinGW-w64 GCC are required. GCC 14.2.0 was locally tested. Put GCC on PATH, set `CC`, or specify an executable:

```powershell
./build.ps1
./test.ps1
# alternatively:
./build.ps1 -Gcc 'D:/tools/mingw64/bin/gcc.exe'
```

Tests use only newly compiled fake DLLs and injected IO boundaries. They do **not** load the original DLL, enumerate/access hardware, or start a host. Build outputs belong in ignored `build/`; release artifacts belong in ignored `dist/`. See [release checklist](releasing.md).

The included Actions workflow is not evidence of a completed GitHub run until you upload the repository and review its result.

## Design and limitations

- Copy RGB18 before returning; never retain the caller's pointer. Do not forward the original LED colors function.
- Emit only Type 0 LED reports. No Type 1 card-write command exists in this bridge; original forwarded IO is not globally read-only.
- Latest-value queue, at most about29.4Hz; intermediate animation frames can be dropped.
- Cancel timed-out writes, then drain completion before reusing IO storage. A broken driver can leave the worker pending indefinitely; process exit is the final recovery boundary.
- A session-local device mutex prevents simultaneous bridge writes. Idle500ms lease release and1000ms cooldown permit another writer and replay unchanged colors after reopening. Multiple processes can still alternate different desired colors; no priority/fairness guarantee.
- Original input receive/reconnect/exception risks are unchanged. Unrelated tools and other sessions are not excluded.
- The module is pinned for process lifetime. No runtime unloading/hot replacement.
- Locally built DLLs are unsigned. Do not disable security software blindly or assume administrator privileges are required.

On one matching controller, an operator confirmed all six lamp positions/colors and final off. A bridged backend ran about119s without a new application error; a12s input probe returned584 samples with connection maintained. This is **not** full gameplay, long-duration, multi-device, or physical hotplug certification. See [test matrix](testing.md) and [architecture](architecture.md).

## Rollback and licensing

Exit all processes and restore backed-up loader paths. Returning to the original also restores its LED behavior. To isolate LEDs while retaining the bridge, prefer `enabled=0` and restart; do not restore unrelated stale UDP configuration.

Bridge code is [MIT-licensed](../LICENSE). No original IO, firmware, game software or private logs are distributed. They are not relicensed by this project. See [NOTICE](../NOTICE.md) for protocol/ABI references. Use only with devices and software you are entitled to use.
