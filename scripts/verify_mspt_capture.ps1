# AFTER a fresh authorized smoke test has stopped normally. No server control.
param([Parameter(Mandatory)][string]$SandboxRoot,
      [Parameter(Mandatory)][string]$LiveReport)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
. (Join-Path $PSScriptRoot 'validate_closed_sqlite.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Assert-BdsSandboxPath $project $SandboxRoot
if ([IO.Path]::GetFileName($root) -ceq 'run-2bbb60dddd2344c3923ae1cbd66fbb98') { throw 'Protected user-test root is not a new MSPT smoke test' }
Assert-BdsUnredirectedTree $root
foreach ($path in @(Get-BdsRunningExecutablePaths)) {
    if ($path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Owned test BDS is still running; do not stop it from this verifier' }
}
$runtime=Read-BdsJsonSnapshot (Join-Path $root 'capture-runtime-evidence.json')
$stage=Read-BdsJsonSnapshot (Join-Path $root 'capture-staging-evidence.json')
$r=$runtime.data
if ($r.sandbox_root -ine $root -or $stage.data.sandbox_root -ine $root -or
    $r.exit_code -ne 0 -or $r.forced_termination -ne $false -or $r.stop_command_issued -ne $true -or
    $r.bds_load_verified -ne $true) { throw 'Missing normal-stop acceptance; no database copied' }
$profilePath=Join-Path $project 'packaging/bds-profile.json'
$profile=Read-BdsJsonSnapshot $profilePath
$candidate=[IO.Path]::GetFullPath($stage.data.candidate_root).TrimEnd('\')
if ([IO.Path]::GetDirectoryName($candidate) -ine ($project+'\.build\server-candidate') -or
    [IO.Path]::GetFileName($candidate) -cnotmatch '^run-[a-f0-9]{32}$') { throw 'Unowned candidate' }
$build=Read-BdsJsonSnapshot (Join-Path $candidate 'candidate-evidence.json')
if ($build.sha256 -ine $stage.data.candidate_evidence_sha256) { throw 'Candidate evidence changed' }
$dllHash=(Get-FileHash -LiteralPath (Join-Path $root 'mods/BedrockInsight/BedrockInsight.dll')).Hash
Assert-BdsCaptureBuild $build.data $profile.data $profile.sha256 $dllHash
if ($r.candidate_dll_sha256 -ine $dllHash -or
    $r.executable_sha256 -ine (Get-FileHash -LiteralPath (Join-Path $root 'bedrock_server_mod.exe')).Hash) { throw 'Test executable/DLL mismatch' }
$livePath=[IO.Path]::GetFullPath($LiveReport)
Assert-BdsUnredirectedAncestors $livePath
if (-not $livePath.StartsWith($project+'\.build\mspt-evidence\',[StringComparison]::OrdinalIgnoreCase) -or
    ([IO.FileInfo]::new($livePath)).Length -gt 1048576) { throw 'Expected bounded live JSON report under .build/mspt-evidence' }
$live=Read-BdsJsonSnapshot $livePath
$db=Join-Path $root 'mods/BedrockInsight/data/bedrock_insight.sqlite3'
# Deny writers/deletion for the original DB for the entire copy/verification.
# immutable=1 is used ONLY on the new private CLOSED COPY, never this source.
$lock=[IO.File]::Open($db,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    Assert-BdsClosedSqliteFiles $db
    if ($lock.Length -gt 268435456) { throw 'Closed snapshot exceeds256MiB budget' }
    $dbHash=(Get-FileHash -LiteralPath $db).Hash
    $evidence=Join-Path $project ('.build/mspt-evidence/run-'+[guid]::NewGuid().ToString('N'))
    Assert-BdsUnredirectedAncestors $evidence
    New-Item -ItemType Directory -Path $evidence | Out-Null
    $copy=Join-Path $evidence 'closed_snapshot.sqlite3'
    Copy-Item -LiteralPath $db -Destination $copy
    Assert-BdsClosedSqliteFiles $db
    if ((Get-FileHash -LiteralPath $copy).Hash -ine $dbHash) { throw 'Closed snapshot copy mismatch' }
    Push-Location -LiteralPath $project
    try {
        Invoke-BdsLoggedNative (Get-Command python.exe).Source @('-m','companion.mspt_acceptance',
            '--snapshot',$copy,'--runtime',$runtime.path,'--live-report',$live.path,'--source',$db) (Join-Path $evidence 'consistency.json')
    } finally { Pop-Location }
    if ((Get-FileHash -LiteralPath $db).Hash -ine $dbHash) { throw 'Original database changed' }
    Assert-BdsClosedSqliteFiles $db
    foreach ($snapshot in @($runtime,$stage,$profile,$build,$live)) { Assert-BdsJsonSnapshotUnchanged $snapshot }
    [pscustomobject]@{verified_utc=[DateTime]::UtcNow.ToString('o');sandbox_root=$root;
        database_sha256=$dbHash;database_unchanged=$true;candidate_dll_sha256=$dllHash;
        runtime_evidence_sha256=$runtime.sha256;staging_evidence_sha256=$stage.sha256;
        live_report_sha256=$live.sha256;consistency_check_passed=$true;
        limits='Evidence consistency only; requires genuine authorized host capture. No independent attestation against edited files or browser screenshot.'} |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $evidence 'acceptance.json') -Encoding UTF8
    Get-Content -LiteralPath (Join-Path $evidence 'consistency.json')
    "MSPT runtime/SQLite/live API/Japanese DOM/normal-stop consistency PASS. Evidence: $evidence"
} finally { $lock.Dispose() }
