# Marker-file fixtures ONLY, no SQLite/BDS process or original server files.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_closed_sqlite.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $project ('.build/closed-sqlite-tests/run-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$cases=@(@(),@('-wal'),@('-shm'),@('-wal','-shm'))
$rejected=0
for ($index=0;$index -lt $cases.Count;$index++) {
    $fixture=Join-Path $root ('case'+$index+'.sqlite3')
    New-Item -ItemType File -Path $fixture | Out-Null
    foreach ($suffix in $cases[$index]) { New-Item -ItemType File -Path ($fixture+$suffix) | Out-Null }
    $lock=[IO.File]::Open($fixture,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $failed=$false
        try { Assert-BdsClosedSqliteFiles $fixture } catch { $failed=$true }
        if ($index -eq 0 -and $failed) { throw 'Closed marker fixture rejected' }
        if ($index -ne 0 -and -not $failed) { throw "Sidecar marker case$index accepted" }
        if ($failed) { ++$rejected }
    } finally { $lock.Dispose() }
}
# Explicitly prove both wrappers call this exact guard after the source lock
# and before Copy-Item; check ordinal source positions, never launch wrappers.
foreach ($name in @('verify_mspt_capture.ps1','verify_real_player_capture.ps1')) {
    $text=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot $name)
    $guard=$text.IndexOf('Assert-BdsClosedSqliteFiles $db',[StringComparison]::Ordinal)
    $lock=$text.IndexOf('$dbLock=[IO.File]::Open',[StringComparison]::Ordinal)
    if ($lock -lt 0) { $lock=$text.IndexOf('$lock=[IO.File]::Open',[StringComparison]::Ordinal) }
    $copy=$text.IndexOf('Copy-Item -LiteralPath $db',[StringComparison]::Ordinal)
    if ($lock -lt 0 -or $guard -le $lock -or $copy -le $guard) { throw "Source lock/sidecar guard/copy ordering regressed: $name" }
}
"Closed SQLite guard PASS:1 absent-sidecar fixture, $rejected independent sidecar rejections,2 wrapper order checks; marker fixtures, not BDS acceptance. Evidence: $root"
