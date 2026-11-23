Set-StrictMode -Version Latest
function Get-ReadonlyInventoryFingerprint([string]$Path) {
    $ticks = (Get-Item -LiteralPath $Path -ErrorAction Stop).LastWriteTimeUtc.Ticks
    $hash = (Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash
    if ((Get-Item -LiteralPath $Path -ErrorAction Stop).LastWriteTimeUtc.Ticks -ne $ticks) {
        throw 'Source timestamp changed during baseline capture'
    }
    return @{ Path = $Path; Hash = $hash; Ticks = $ticks }
}
function Assert-ReadonlyInventoryUnchanged($Baseline) {
    $current = Get-ReadonlyInventoryFingerprint $Baseline.Path
    if ($current.Hash -cne $Baseline.Hash -or $current.Ticks -ne $Baseline.Ticks) {
        throw 'Read-only inventory changed physical source bytes or timestamp'
    }
}
