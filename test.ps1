[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'scripts\common.ps1')
Push-Location -LiteralPath $PSScriptRoot
$runRoot = $null
try {
    Assert-BridgeBuildManifest -Root $PSScriptRoot
    $build = Join-Path $PSScriptRoot 'build'
    # Never use an existing deployment/game directory or an old fake sandbox.
    $runRoot = Join-Path $build ('software-tests-' + [guid]::NewGuid().ToString('N'))
    $fake = Join-Path $runRoot 'fake'
    $missing = Join-Path $runRoot 'missing'
    New-Item -ItemType Directory -Path $fake, $missing -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'fake/MU3Input.dll') -Destination (Join-Path $fake 'MU3Input.dll')
    foreach ($dir in @($fake, $missing)) {
        Copy-Item -LiteralPath (Join-Path $build 'MU3Input.LegacyBridge.dll') -Destination $dir
        # Production bridge defaults to LED enabled. Explicitly disable it BEFORE
        # LoadLibrary: no enumeration, writer thread, or physical HID access.
        Set-Content -LiteralPath (Join-Path $dir 'MU3Input.LegacyBridge.ini') -Value "[bridge]`r`nenabled=0`r`nbrightness=40" -Encoding ASCII
    }
    if (Test-Path -LiteralPath (Join-Path $missing 'MU3Input.dll')) { throw 'Unsafe missing-DLL sandbox: unexpected MU3Input.dll.' }
    $sourceHash = (Get-FileHash -LiteralPath (Join-Path $build 'fake/MU3Input.dll') -Algorithm SHA256).Hash
    if ((Get-FileHash -LiteralPath (Join-Path $fake 'MU3Input.dll') -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Test double copy hash mismatch.' }
    # Pure encode and fault-injected internals; these executables do not open HID.
    & (Join-Path $build 'test_encode.exe')
    Assert-BridgeExit 'encode' $LASTEXITCODE
    & (Join-Path $build 'test_lifetime.exe')
    Assert-BridgeExit 'lifetime' $LASTEXITCODE
    & (Join-Path $build 'test_scheduler.exe')
    Assert-BridgeExit 'scheduler' $LASTEXITCODE
    Push-Location -LiteralPath $fake
    try {
        & (Join-Path $build 'test_forward.exe') (Join-Path $fake 'MU3Input.LegacyBridge.dll') 'fake'
        Assert-BridgeExit 'forward' $LASTEXITCODE
        # Deliberately CWD=fake: missing sibling must NOT fall back to CWD DLL.
        & (Join-Path $build 'test_forward.exe') (Join-Path $missing 'MU3Input.LegacyBridge.dll') 'missing'
        Assert-BridgeExit 'missing sibling' $LASTEXITCODE
    }
    finally { Pop-Location }
    Write-Output 'PASS all 5 tests: encode, lifetime, scheduler, forwarding, missing sibling; fake-only, no original runtime/game/hardware'
}
finally {
    Pop-Location
    if ($runRoot -and (Test-Path -LiteralPath $runRoot)) {
        # Only remove the exact uniquely named sandbox allocated above.
        $resolved = (Resolve-Path -LiteralPath $runRoot).ProviderPath
        $expectedParent = Join-Path $PSScriptRoot 'build'
        if ((Split-Path -Parent $resolved) -ne $expectedParent -or (Split-Path -Leaf $resolved) -notmatch '^software-tests-[0-9a-f]{32}$') {
            throw "Refusing unexpected cleanup target: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
