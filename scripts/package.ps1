[CmdletBinding()]
param(
    [string]$Gcc,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$Version = 'dev',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'common.ps1')
# Forward caller-relative compiler paths before changing CWD.
if (!$SkipBuild) { $compiler = Resolve-BridgeGcc -Gcc $Gcc }
Push-Location -LiteralPath $root
try {
    if (!$SkipBuild) { & './build.ps1' -Gcc $compiler }
    # A package is never produced from untested/stale/fake-substituted outputs.
    & './test.ps1'
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    # Explicit allowlist: never recursively archive build, repository, or staging.
    $entries = [ordered]@{
        'MU3Input.LegacyBridge.dll' = 'build/MU3Input.LegacyBridge.dll'
        'MU3Input.LegacyBridge.ini' = 'config/MU3Input.LegacyBridge.ini'
        'README.md' = 'README.md'
        'LICENSE' = 'LICENSE'
        'NOTICE.md' = 'NOTICE.md'
        'CHANGELOG.md' = 'CHANGELOG.md'
        'docs/README.en.md' = 'docs/README.en.md'
        'docs/architecture.md' = 'docs/architecture.md'
        'docs/testing.md' = 'docs/testing.md'
        'docs/releasing.md' = 'docs/releasing.md'
    }
    foreach ($source in $entries.Values) {
        if (!(Test-Path -LiteralPath $source -PathType Leaf)) { throw "Required package input missing: $source" }
        if ((Get-Item -LiteralPath $source).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing linked package input: $source" }
    }
    New-Item -ItemType Directory -Force -Path 'dist' | Out-Null
    $archivePath = Join-Path $root "dist/MU3Input-LegacyBridge-$Version-win64.zip"
    $stream = [IO.File]::Open($archivePath, [IO.FileMode]::Create, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    try {
        $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
        try {
            foreach ($name in $entries.Keys) {
                $entry = $zip.CreateEntry($name, [IO.Compression.CompressionLevel]::Optimal)
                # Stable ZIP metadata, too. Byte identity is scoped to the same
                # toolchain/.NET version, source and settings, not all GCC versions.
                $entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
                $inputStream = [IO.File]::OpenRead((Join-Path $root $entries[$name]))
                try {
                    $outputStream = $entry.Open()
                    try { $inputStream.CopyTo($outputStream) }
                    finally { $outputStream.Dispose() }
                }
                finally { $inputStream.Dispose() }
            }
        }
        finally { $zip.Dispose() }
    }
    finally { $stream.Dispose() }
    $hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($archivePath))" | Set-Content -LiteralPath ($archivePath + '.sha256') -Encoding ASCII
    Write-Output "PASS package allowlist ($($entries.Count) files): $archivePath"
    Write-Output 'Excluded: original MU3Input.dll, fake DLL, test binaries, logs, sources and build tree.'
}
finally { Pop-Location }
