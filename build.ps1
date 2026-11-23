[CmdletBinding()]
param([string]$Gcc)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
$compiler = Resolve-BridgeGcc -Gcc $Gcc
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Force -Path 'build', 'build/fake' | Out-Null
    # Invalidate previous success before compiling: a partial/failed rebuild must
    # never leave the last run's manifest eligible for testing or packaging.
    Set-Content -LiteralPath 'build/software-tests.json' -Value '{"format":0,"files":[]}' -Encoding UTF8
    # Keep EVERY GCC input/output argument relative: MinGW's linker can mangle
    # Unicode absolute paths. bridge.c's _Static_assert rejects non-x64 builds.
    # Do not define NDEBUG: every software test relies on C assert().
    $flags = @('-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-static-libgcc', '-Wl,--no-insert-timestamp')
    & $compiler @flags '-shared' 'src/bridge.c' 'src/encode.c' 'src/bridge.def' '-o' 'build/MU3Input.LegacyBridge.dll' '-lhid' '-lsetupapi'
    Assert-BridgeExit 'bridge compile (requires x64)' $LASTEXITCODE
    & $compiler @flags 'tests/test_encode.c' 'src/encode.c' '-o' 'build/test_encode.exe'
    Assert-BridgeExit 'encode test compile' $LASTEXITCODE
    & $compiler @flags '-municode' 'tests/test_forward.c' '-o' 'build/test_forward.exe'
    Assert-BridgeExit 'forward test compile' $LASTEXITCODE
    & $compiler @flags '-shared' 'tests/fake_original.c' '-o' 'build/fake/MU3Input.dll'
    Assert-BridgeExit 'software test double compile' $LASTEXITCODE
    & $compiler @flags 'tests/test_lifetime.c' 'src/encode.c' '-o' 'build/test_lifetime.exe' '-lhid' '-lsetupapi'
    Assert-BridgeExit 'lifetime test compile' $LASTEXITCODE
    & $compiler @flags 'tests/test_scheduler.c' 'src/encode.c' '-o' 'build/test_scheduler.exe' '-lhid' '-lsetupapi'
    Assert-BridgeExit 'scheduler test compile' $LASTEXITCODE
    # Detect accidental replacement by a real/vendor DLL before any test loads it.
    # This is a local consistency check, NOT an authenticity/security signature.
    $files = @('MU3Input.LegacyBridge.dll', 'test_encode.exe', 'test_forward.exe', 'test_lifetime.exe', 'test_scheduler.exe', 'fake/MU3Input.dll')
    $entries = @($files | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path 'build' $_) -Algorithm SHA256).Hash }
    })
    $sourceEntries = @(Get-BridgeSourcePaths | ForEach-Object {
        [ordered]@{ path = $_; sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash }
    })
    [ordered]@{ format = 2; files = $entries; sources = $sourceEntries } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath 'build/software-tests.json' -Encoding UTF8
    Write-Output 'PASS build: x64 source assertion; -Wall -Wextra -Werror; deterministic PE timestamps; software tests built'
}
finally { Pop-Location }
