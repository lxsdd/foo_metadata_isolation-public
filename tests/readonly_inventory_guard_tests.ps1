$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'readonly_inventory_guard.ps1')
$root = Join-Path ([IO.Path]::GetTempPath()) ('mi-inventory-guard-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($root)
try {
    $path = Join-Path $root 'synthetic.bin'
    [IO.File]::WriteAllBytes($path, [byte[]](1, 2, 3))
    $baseline = Get-ReadonlyInventoryFingerprint $path
    Assert-ReadonlyInventoryUnchanged $baseline
    # A timestamp-only mutation is invisible to SHA-256 and must fail.
    [IO.File]::SetLastWriteTimeUtc($path, [DateTime]::new($baseline.Ticks, [DateTimeKind]::Utc).AddSeconds(10))
    $rejected = $false
    try { Assert-ReadonlyInventoryUnchanged $baseline } catch { $rejected = $true }
    if (-not $rejected) { throw 'Timestamp-only mutation escaped inventory guard' }
    # A byte mutation with restored timestamp must also fail.
    [IO.File]::WriteAllBytes($path, [byte[]](3, 2, 1))
    [IO.File]::SetLastWriteTimeUtc($path, [DateTime]::new($baseline.Ticks, [DateTimeKind]::Utc))
    $rejected = $false
    try { Assert-ReadonlyInventoryUnchanged $baseline } catch { $rejected = $true }
    if (-not $rejected) { throw 'Same-timestamp byte mutation escaped inventory guard' }
    Write-Host 'READONLY_INVENTORY_GUARD_POSITIVE_NEGATIVE=PASS'
} finally { Remove-Item -LiteralPath $root -Recurse -Force }
