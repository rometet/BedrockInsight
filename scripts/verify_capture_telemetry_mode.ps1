# Invalid parameters must fail before staging, file changes or process start.
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$absent=Join-Path $project ('.build/isolated-bds/run-' + [guid]::NewGuid().ToString('N'))
$stage=Join-Path $PSScriptRoot 'stage_isolated_capture.ps1'
$runner=Join-Path $PSScriptRoot 'run_isolated_capture.ps1'
$count=0
foreach($parameters in @(
    @{CandidateRoot=$absent;ServerPort=49151},
    @{CandidateRoot=$absent;ServerPortV6=65536},
    @{CandidateRoot=$absent;ServerPort=59582;ServerPortV6=59582}
)) {
    $rejected=$false
    try { & $stage @parameters } catch {$rejected=$true}
    if(-not $rejected -or (Test-Path -LiteralPath $absent)){throw 'Stage invalid parameters reached side effects'}
    ++$count
}
foreach($parameters in @(
    @{SandboxRoot=$absent;SmokeHoldSeconds=2},
    @{SandboxRoot=$absent;SmokeHoldSeconds=31},
    @{SandboxRoot=$absent;RealPlayer=$true;SmokeHoldSeconds=15}
)) {
    $rejected=$false
    try { & $runner @parameters } catch {$rejected=$true}
    if(-not $rejected -or (Test-Path -LiteralPath $absent)){throw 'Runner invalid parameters reached side effects'}
    ++$count
}
"Telemetry isolation parameter rejection tests: PASS ($count); no stage/process created"
