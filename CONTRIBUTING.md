# Contributing

Please discuss protocol/ABI changes before broadening device support. A matching VID/PID alone is not evidence of compatibility.

1. Build and run all offline tests on Windows x64. Do not replace fake fixtures with vendor DLLs.
2. Keep buffer/OVERLAPPED lifetime, cancellation drain and same-thread mutex ownership explicit. Do not do blocking IO in a host callback or DllMain.
3. Add regression coverage for changed production logic; distinguish simulation from hardware tests.
4. Keep changes focused; describe compiler, test commands and what was not tested. New hardware IO needs the operator's informed consent and a safe recovery plan.
5. Do not submit game binaries, original IO/firmware, private logs, real device identifiers, card data or extracted third-party implementation.
6. Include provenance and applicable notices for third-party code. Publicly readable code is not automatically relicensable. Do not claim this repository's MIT covers upstream components.

Synthetic fixture values in test sources are intentionally fabricated boundary data, not actual card/device identities.

Contributions are intended under the repository MIT license; submit only material you are entitled to license. No CLA or external upload service is configured here.
