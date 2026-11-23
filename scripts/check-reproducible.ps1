[CmdletBinding()]
param([string]$Gcc)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'common.ps1')
$compiler = Resolve-BridgeGcc -Gcc $Gcc
Push-Location -LiteralPath $root
try {
    $files = @('MU3Input.LegacyBridge.dll', 'test_encode.exe', 'test_forward.exe', 'test_lifetime.exe', 'test_scheduler.exe', 'fake/MU3Input.dll')
    & './build.ps1' -Gcc $compiler
    $before = @{}
    foreach ($file in $files) { $before[$file] = (Get-FileHash -LiteralPath (Join-Path 'build' $file) -Algorithm SHA256).Hash }
    # A timestamp-changing delay is unnecessary: independently inspect that all
    # PE COFF timestamps are zero as well as comparing two complete builds.
    & './build.ps1' -Gcc $compiler
    foreach ($file in $files) {
        $path = Join-Path $root (Join-Path 'build' $file)
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $before[$file]) { throw "Non-reproducible build: $file" }
        $bytes = [IO.File]::ReadAllBytes($path)
        $pe = [BitConverter]::ToInt32($bytes, 0x3c)
        if ([BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550 -or [BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { throw "Not an x64 PE: $file" }
        if ([BitConverter]::ToUInt32($bytes, $pe + 8) -ne 0) { throw "Nonzero PE timestamp: $file" }
        Write-Output "PASS reproducible x64 PE: $file SHA256=$($before[$file])"
    }
}
finally { Pop-Location }
