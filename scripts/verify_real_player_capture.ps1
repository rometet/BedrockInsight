# Read-only verification AFTER an isolated real-client run has cleanly exited.
param([Parameter(Mandatory)][string]$SandboxRoot,
      [Parameter(Mandatory)][string]$PlayerName,
      [Parameter(Mandatory)][string]$PlayerXuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
. (Join-Path $PSScriptRoot 'real_player_capture_policy.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
. (Join-Path $PSScriptRoot 'validate_closed_sqlite.ps1')
$access=New-BdsCaptureAccessPolicy $true $PlayerName $PlayerXuid
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Assert-BdsSandboxPath $project $SandboxRoot
Assert-BdsUnredirectedTree $root
foreach ($path in @(Get-BdsRunningExecutablePaths)) {
    if ($path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Stop the owned test BDS normally before inspecting its database' }
}
$runtimeSnapshot=Read-BdsJsonSnapshot (Join-Path $root 'capture-runtime-evidence.json')
$runtime=$runtimeSnapshot.data
$stagingSnapshot=Read-BdsJsonSnapshot (Join-Path $root 'capture-staging-evidence.json')
Assert-BdsCaptureAccessPolicy $stagingSnapshot.data.capture_access $access
Assert-BdsCaptureAccessPolicy $runtime.capture_access $access
Assert-BdsCaptureAccessPolicy $runtime.preflight.capture_access $access
if ($runtime.startup_outcome.stop_reason -cne 'player_roundtrip_observed' -or
    -not (Test-BdsRealPlayerWindow $runtime.player_window_seconds)) {
    throw 'Missing bounded real-player runtime acceptance (zero-player smoke evidence is not sufficient)'
}
if ($runtime.sandbox_root -ine $root -or $runtime.exit_code -ne 0 -or $runtime.error -or
    $runtime.forced_termination -isnot [bool] -or $runtime.forced_termination) { throw 'Missing clean isolated runtime exit evidence' }
foreach ($flag in @('native_loaded','native_enabled','server_started','stop_command_issued','bds_load_verified','loader_config_migrated_with_disabled_overrides')) {
    if ($runtime.$flag -isnot [bool] -or -not $runtime.$flag) { throw "Missing runtime acceptance: $flag" }
}
$dll=Join-Path $root 'mods/BedrockInsight/BedrockInsight.dll'
if ((Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash -ine $runtime.candidate_dll_sha256) { throw 'Runtime mod DLL hash mismatch' }
if (-not $env:BEDROCKINSIGHT_SQLITE_ROOT) { throw 'BEDROCKINSIGHT_SQLITE_ROOT is required (installed SQLite CLI; never download or modify another project)' }
$sqlite=Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'bin/sqlite3.exe'
if (-not [IO.File]::Exists($sqlite)) { throw 'Installed SQLite CLI missing' }
$reportRoot=Join-Path $project ('.build/real-player-evidence/run-'+[guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $reportRoot
New-Item -ItemType Directory -Path $reportRoot | Out-Null
$db=Join-Path $root 'mods/BedrockInsight/data/bedrock_insight.sqlite3'
$dbLock=[IO.File]::Open($db,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    Assert-BdsClosedSqliteFiles $db
    $dbHash=(Get-FileHash -LiteralPath $db -Algorithm SHA256).Hash
    # Query a private closed copy, so even SQLite reader-side files cannot be
    # created next to the original stopped server database.
    $queryDb=Join-Path $reportRoot 'closed_snapshot.sqlite3'
    Copy-Item -LiteralPath $db -Destination $queryDb
    Assert-BdsClosedSqliteFiles $db
    if ((Get-FileHash -LiteralPath $queryDb).Hash -ine $dbHash) { throw 'Closed player snapshot copy mismatch' }
    function Read-Query([string]$Sql,[string]$Name) {
        $log=Join-Path $reportRoot ($Name+'.json')
        # No input value, player name, filename or user-supplied SQL is inserted
        # into SQL. All filtering of the bounded result happens in PowerShell.
        Invoke-BdsLoggedNative $sqlite @('-readonly','-json',$queryDb,$Sql) $log
        $text=Get-Content -Raw -LiteralPath $log
        if (-not $text -or -not $text.Trim()) { return @() }
        return @($text | ConvertFrom-Json)
    }
    $integrity=@(Read-Query 'PRAGMA integrity_check;' 'integrity')
    $foreignKeys=@(Read-Query 'PRAGMA foreign_key_check;' 'foreign-keys')
    $schema=@(Read-Query 'PRAGMA user_version;' 'schema')
    $events=@(Read-Query 'SELECT event_id,occurred_at_ms,event_kind,actor_kind,actor_id,actor_name,session_id,cause FROM events ORDER BY event_id LIMIT 1001;' 'events')
    $sessions=@(Read-Query 'SELECT session_id,player_id,started_at_ms,ended_at_ms,duration_ms FROM sessions ORDER BY started_at_ms LIMIT 1001;' 'sessions')
    if ($events.Count -gt 1000 -or $sessions.Count -gt 1000) { throw 'Private smoke test exceeds the bounded row budget; inspect separately' }
    $startMs=[DateTimeOffset]::Parse($runtime.started_utc).ToUnixTimeMilliseconds()
    $endMs=[DateTimeOffset]::Parse($runtime.finished_utc).ToUnixTimeMilliseconds()
    $pair=Test-BdsRealPlayerRows $events $sessions $PlayerName $startMs $endMs
    $stdoutPath=Join-Path $root 'capture-stdout.log'
    $stdout=Get-Content -Raw -Encoding UTF8 -LiteralPath $stdoutPath
    $logPair=Get-BdsCapturePlayerLogPair @($stdout -split '\r?\n') $access
    $connected=($logPair.connect_count -eq 1)
    $disconnected=($logPair.disconnect_count -eq 1)
    $disabled=$stdout.Contains('Disabling BedrockInsight')
    $quit=$stdout.Contains('Quit correctly')
    $integrityOk=($integrity.Count -eq 1 -and $integrity[0].integrity_check -ceq 'ok')
    $schemaOk=($schema.Count -eq 1 -and $schema[0].user_version -eq 4)
    $passed=($pair.database_pair_verified -and $logPair.pair_observed -and $disabled -and $quit -and
        $integrityOk -and $schemaOk -and $foreignKeys.Count -eq 0)
    if ((Get-FileHash -LiteralPath $db -Algorithm SHA256).Hash -ine $dbHash) { throw 'Database changed during read-only verification' }
    Assert-BdsClosedSqliteFiles $db
    Assert-BdsJsonSnapshotUnchanged $runtimeSnapshot
    Assert-BdsJsonSnapshotUnchanged $stagingSnapshot
    $result=[pscustomobject]@{verified_utc=[DateTime]::UtcNow.ToString('o');sandbox_root=$root;
        database=$db;database_snapshot=$queryDb;database_sha256=$dbHash;database_unchanged=$true;
        runtime_evidence_sha256=$runtimeSnapshot.sha256;stdout_sha256=(Get-FileHash -LiteralPath $stdoutPath -Algorithm SHA256).Hash;
        staging_evidence_sha256=$stagingSnapshot.sha256;capture_access=$access;player_log_pair=$logPair;
        candidate_dll_sha256=$runtime.candidate_dll_sha256;expected_player_name=$PlayerName;expected_player_xuid=$PlayerXuid;
        sqlite_cli=$sqlite;sqlite_cli_sha256=(Get-FileHash -LiteralPath $sqlite -Algorithm SHA256).Hash;
        sqlite_integrity_ok=$integrityOk;schema_version_ok=$schemaOk;foreign_key_issues=$foreignKeys.Count;
        event_rows=$events.Count;session_rows=$sessions.Count;player_rows=$pair;
        real_client_connect_log=$connected;real_client_disconnect_log=$disconnected;
        normal_disable_log=$disabled;normal_quit_log=$quit;event_to_db_verified=$passed;
        limits='Database/log consistency for a named real-client test, not independent attestation against edited evidence. Requires an actual authenticated player connection; fixtures or inserted rows do not establish game capture.'}
    $report=Join-Path $reportRoot 'real-player-evidence.json'
    $result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $report -Encoding UTF8
    "Real player evidence: $report"
    if (-not $passed) { [Console]::Error.WriteLine('Real join/leave acceptance NOT proven; inspect missing rows/log observations in report.'); exit 2 }
    'Real join/leave database and normal-shutdown consistency PASS'
} finally { $dbLock.Dispose() }
