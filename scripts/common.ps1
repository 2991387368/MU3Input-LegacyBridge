# Shared helpers. Compatible with Windows PowerShell 5.1 and PowerShell 7.
Set-StrictMode -Version Latest

function Resolve-BridgeGcc {
    param([string]$Gcc)
    $candidate = $Gcc
    if ([string]::IsNullOrWhiteSpace($candidate)) { $candidate = $env:CC }
    if ([string]::IsNullOrWhiteSpace($candidate)) { $candidate = 'gcc' }
    # Resolve BEFORE changing CWD: -Gcc .\toolchain\gcc.exe is caller-relative.
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        return (Resolve-Path -LiteralPath $candidate).ProviderPath
    }
    $command = Get-Command -Name $candidate -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
    if (!$command) {
        throw "GCC not found: '$candidate'. Pass -Gcc <executable>, set CC to an executable path, or add MinGW-w64 gcc to PATH. CC must not contain compiler arguments."
    }
    return $command.Source
}

function Assert-BridgeExit {
    param([string]$Operation, [int]$ExitCode)
    if ($ExitCode -ne 0) { throw "$Operation failed (exit $ExitCode)" }
}

function Get-BridgeSourcePaths {
    # Exact production and test inputs used by build.ps1 (not arbitrary CWD files).
    return @('src/bridge.c', 'src/bridge.h', 'src/bridge.def', 'src/encode.c', 'tests/test_encode.c', 'tests/test_forward.c', 'tests/fake_original.c', 'tests/test_lifetime.c', 'tests/test_scheduler.c', 'build.ps1', 'test.ps1', 'scripts/common.ps1')
}

function Assert-BridgeBuildManifest {
    param([string]$Root)
    $manifestPath = Join-Path $Root 'build\software-tests.json'
    if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw 'Run build.ps1 first: software test manifest is missing.' }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.format -ne 2) { throw 'Incomplete or outdated software test manifest. Run build.ps1 again.' }
    $expected = @('MU3Input.LegacyBridge.dll', 'test_encode.exe', 'test_forward.exe', 'test_lifetime.exe', 'test_scheduler.exe', 'fake/MU3Input.dll')
    $actual = @($manifest.files | ForEach-Object { $_.path })
    $sourcePaths = @(Get-BridgeSourcePaths)
    $actualSources = @($manifest.sources | ForEach-Object { $_.path })
    if ($manifest.format -ne 2 -or $actual.Count -ne $expected.Count -or @(Compare-Object $expected $actual).Count -ne 0 -or $actualSources.Count -ne $sourcePaths.Count -or @(Compare-Object $sourcePaths $actualSources).Count -ne 0) {
        throw 'Unexpected software test manifest. Run build.ps1 again.'
    }
    foreach ($entry in $manifest.sources) {
        $file = Join-Path $Root $entry.path
        if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Source/build script changed since the build: $($entry.path). Run build.ps1 again."
        }
    }
    foreach ($entry in $manifest.files) {
        $file = Join-Path (Join-Path $Root 'build') $entry.path
        if (!(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256) {
            throw "Build output differs from locally built software test manifest: $($entry.path). Refusing to load a potentially real or stale DLL; run build.ps1 again."
        }
    }
}
