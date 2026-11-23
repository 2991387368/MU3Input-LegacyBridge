# Changelog

## 0.1.0 — public preparation

- Extract the previously validated native bridge into a standalone public source tree; no change to production C code or test C fixtures in this cleanup.
- Forward MU3Input 3.1 input/Aime functions; translate RGB18 to legacy slots0–2/5–7 and zero all remaining report bytes.
- Dedicated overlapped HID writer with cancellation drain, same-session device mutex, bounded brightness, idle lease release and same-color replay after reopen.
- Five offline software-test groups; portable PowerShell GCC selection, deterministic PE timestamps, allowlisted binary packaging and Windows CI artifact workflow.
- Chinese/English documentation, explicit test limitations, MIT license and upstream attribution boundaries.

This is a preparation version, not a claim that a GitHub release/tag or remote CI run exists. Historical hardware observations cover one controller, six lamp positions/colors and short backend/input checks; no full gameplay, endurance or physical hotplug certification.
