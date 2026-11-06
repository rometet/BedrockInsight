# Only a precondition, NOT SQLite integrity/runtime evidence. No I/O writes.
function Assert-BdsClosedSqliteFiles([string]$DatabasePath) {
    foreach ($suffix in @('-wal','-shm')) {
        $sidecar=$DatabasePath+$suffix
        if ([IO.File]::Exists($sidecar) -or [IO.Directory]::Exists($sidecar)) {
            throw "WAL/SHM still present: not a closed database snapshot: $sidecar"
        }
    }
}
