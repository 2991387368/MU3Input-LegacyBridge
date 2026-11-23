[CmdletBinding()]
param([ValidatePattern('^[A-Za-z0-9][A-Za-z0-9._-]{0,79}$')][string]$Version = 'dev')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
# Explicit public source allowlist. Never include .git, build, dist, evidence or vendor binaries.
$files = @(
    '.gitignore', '.gitattributes', '.github/workflows/build.yml',
    'README.md', 'LICENSE', 'NOTICE.md', 'CHANGELOG.md', 'CONTRIBUTING.md', 'SECURITY.md',
    'build.ps1', 'test.ps1', 'config/MU3Input.LegacyBridge.ini',
    'src/bridge.c', 'src/bridge.h', 'src/bridge.def', 'src/encode.c',
    'tests/fake_original.c', 'tests/test_encode.c', 'tests/test_forward.c', 'tests/test_lifetime.c', 'tests/test_scheduler.c',
    'scripts/common.ps1', 'scripts/check-reproducible.ps1', 'scripts/package.ps1', 'scripts/package-source.ps1',
    'docs/README.en.md', 'docs/architecture.md', 'docs/testing.md', 'docs/releasing.md'
)
foreach ($name in $files) {
    $path = Join-Path $root $name
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing source input: $name" }
    if ((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Refusing linked source input: $name" }
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
New-Item -ItemType Directory -Path (Join-Path $root 'dist') -Force | Out-Null
$destination = Join-Path $root "dist/MU3Input-LegacyBridge-$Version-source.zip"
$stream = [IO.File]::Open($destination, [IO.FileMode]::Create, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $zip = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        foreach ($name in $files) {
            $entry = $zip.CreateEntry(('mu3input-legacy-bridge/' + $name), [IO.Compression.CompressionLevel]::Optimal)
            $entry.LastWriteTime = [DateTimeOffset]::new(1980, 1, 1, 0, 0, 0, [TimeSpan]::Zero)
            $inputStream = [IO.File]::OpenRead((Join-Path $root $name))
            try {
                $outputStream = $entry.Open()
                try { $inputStream.CopyTo($outputStream) } finally { $outputStream.Dispose() }
            } finally { $inputStream.Dispose() }
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
$hash = (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($destination))" | Set-Content -LiteralPath ($destination + '.sha256') -Encoding ASCII
Write-Output "PASS source allowlist ($($files.Count) files): $destination"
