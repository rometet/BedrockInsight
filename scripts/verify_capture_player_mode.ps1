$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
# In-memory metadata/log examples ONLY: no client, SQLite, server files,
# executable fixtures, process launch, network or fabricated gameplay proof.
$smoke=New-BdsCaptureAccessPolicy $false '' ''
$player=New-BdsCaptureAccessPolicy $true 'Example' '1234567890'
Assert-BdsCaptureAccessPolicy $player $player
Assert-BdsCaptureAccessPolicy $smoke $smoke
$json=Get-BdsCaptureAllowlistJson $player
if ($json -cne '[{"name":"Example","xuid":"1234567890","ignoresPlayerLimit":false}]') { throw 'Unexpected allowlist serialization' }
Assert-BdsCapturePlayerAllowlist ($json+"`r`n") $player
if ((Get-BdsCaptureAllowlistJson $smoke) -cne '[]') { throw 'Default smoke allowlist changed' }
$rejections=@(
    { New-BdsCaptureAccessPolicy $false 'Example' '' },
    { New-BdsCaptureAccessPolicy $false '' '123' },
    { New-BdsCaptureAccessPolicy $true '' '123' },
    { New-BdsCaptureAccessPolicy $true ' Example' '123' },
    { New-BdsCaptureAccessPolicy $true 'Example ' '123' },
    { New-BdsCaptureAccessPolicy $true 'Example,Other' '123' },
    { New-BdsCaptureAccessPolicy $true "Example`nOther" '123' },
    { New-BdsCaptureAccessPolicy $true ('a'*65) '123' },
    { New-BdsCaptureAccessPolicy $true 'Example' '' },
    { New-BdsCaptureAccessPolicy $true 'Example' '0' },
    { New-BdsCaptureAccessPolicy $true 'Example' '0123' },
    { New-BdsCaptureAccessPolicy $true 'Example' '-1' },
    { New-BdsCaptureAccessPolicy $true 'Example' '18446744073709551616' },
    { Assert-BdsCaptureAccessPolicy $null $player },
    { Assert-BdsCaptureAccessPolicy $smoke $player },
    { Assert-BdsCaptureAccessPolicy $player $smoke },
    { Assert-BdsCaptureAccessPolicy ([pscustomobject]@{mode='empty-smoke';bad=1;other=2}) $smoke },
    { Assert-BdsCaptureAccessPolicy ([pscustomobject]@{mode='one-real-player';player_name='Example';player_xuid=1234567890}) $player },
    { Assert-BdsCapturePlayerAllowlist '[]' $player },
    { Assert-BdsCapturePlayerAllowlist $json.Replace('Example','Other') $player },
    { Assert-BdsCapturePlayerAllowlist $json.Replace('1234567890','1234567891') $player },
    { Assert-BdsCapturePlayerAllowlist $json.Replace('false','true') $player },
    { Assert-BdsCapturePlayerAllowlist ($json.TrimEnd(']')+','+$json.TrimStart('[')) $player },
    { Assert-BdsCapturePlayerAllowlist $json.Replace('"xuid":"1234567890",','') $player },
    { Assert-BdsCapturePlayerAllowlist $json.Replace('"xuid":','"xuid":"999","xuid":') $player },
    { Get-BdsCaptureWindowDecision $player -1 120 @() },
    { Get-BdsCaptureWindowDecision $player 0 29 @() },
    { Get-BdsCaptureWindowDecision $player 0 3601 @() }
)
foreach ($case in $rejections) {
    $rejected=$false
    try { & $case | Out-Null } catch { $rejected=$true }
    if (-not $rejected) { throw 'Expected explicit player access/window rejection' }
}
$max=New-BdsCaptureAccessPolicy $true 'Example' '18446744073709551615'
Assert-BdsCapturePlayerAllowlist (Get-BdsCaptureAllowlistJson $max) $max
$join='02:00:00.000 IFO [Server] Player connected: Example, xuid: 1234567890, pfid: example'
$leave='02:00:10.000 IFO [Server] Player disconnected: Example, xuid: 1234567890, pfid: example'
$cases=@(
    @{access=$smoke;ms=2999;lines=@();reason=$null},
    @{access=$smoke;ms=3000;lines=@();reason='ready_hold_completed'},
    @{access=$player;ms=3000;lines=@();reason=$null},
    @{access=$player;ms=119999;lines=@($join);reason=$null},
    @{access=$player;ms=120000;lines=@();reason='player_window_timeout'},
    @{access=$player;ms=120000;lines=@($join,$leave);reason='player_window_timeout'},
    @{access=$player;ms=10000;lines=@($join,$leave);reason='player_roundtrip_observed'},
    @{access=$player;ms=10000;lines=@($leave,$join);reason='player_sequence_invalid'},
    @{access=$player;ms=10000;lines=@($join,$join,$leave);reason='player_sequence_invalid'},
    @{access=$player;ms=10000;lines=@($join,$leave,$leave);reason='player_sequence_invalid'},
    @{access=$player;ms=10000;lines=@($join.Replace('1234567890','1234567891'),$leave);reason='player_identity_mismatch'},
    @{access=$player;ms=10000;lines=@($join,$leave.Replace('Example','Other'));reason='player_identity_mismatch'},
    @{access=$player;ms=10000;lines=@($join.Replace('[Server]','[Chat]'),$leave.Replace('[Server]','[Chat]'));reason=$null},
    @{access=$player;ms=10000;lines=@('chat says '+$join,'chat says '+$leave);reason=$null}
)
foreach ($case in $cases) {
    $actual=Get-BdsCaptureWindowDecision $case.access $case.ms 120 $case.lines
    if ($actual.stop_reason -cne $case.reason -or $actual.stop -ne ($null -ne $case.reason)) { throw "Incorrect player window decision: $($case.reason)" }
}
# The ordinary boundary must still reject a nonempty list. The explicit
# alternate shares every other physical/configuration check, not a fake
# allowlist_empty=true fact or a bypass of the original checker.
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$id=[guid]::NewGuid().ToString('N')
$facts=[pscustomobject]@{sandbox_root=(Join-Path $project ('.build/isolated-bds/run-'+$id));
    allowlist_empty=$false;allowlist_text=$json;permissions_empty=$true;world_directory_exists=$false;
    mod_directories=@('LeviLamina','BedrockInsight');bound_ports=@();running_bds_paths=@();
    server_properties=@"
transport=nethernet
server-port=59472
server-udp-ports=59472
server-portv6=59473
enable-lan-visibility=false
allow-list=true
online-mode=true
max-players=1
default-player-permission-level=visitor
allow-cheats=false
level-name=insight-isolated-$id
"@}
$checked=Test-BdsIsolationBoundary $facts $project $player
if (-not $checked.candidate_policy_passed -or $checked.launch_authorized) { throw 'Explicit player policy must not itself authorize launch' }
$boundaryRejections=@(
    { Test-BdsIsolationBoundary $facts $project },
    { $x=$facts.psobject.Copy();$x.permissions_empty=$false;Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.world_directory_exists=$true;Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.allowlist_text='[]';Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.server_properties=$x.server_properties.Replace('online-mode=true','online-mode=false');Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.server_properties=$x.server_properties.Replace('visitor','operator');Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.bound_ports=@(59472);Test-BdsIsolationBoundary $x $project $player },
    { $x=$facts.psobject.Copy();$x.running_bds_paths=@($facts.sandbox_root+'\bedrock_server_mod.exe');Test-BdsIsolationBoundary $x $project $player }
)
foreach ($case in $boundaryRejections) {
    $rejected=$false
    try { & $case | Out-Null } catch { $rejected=$true }
    if (-not $rejected) { throw 'Player mode bypassed a default/physical isolation boundary' }
}
# Exercise real entrypoint parameter plumbing using INVALID identities and
# nonexistent owned paths. All must reject BEFORE staging, native tools or
# process construction; there is no executable/server fixture to fall back to.
$missingRoot=Join-Path $project ('.build/isolated-bds/run-'+[guid]::NewGuid().ToString('N'))
$missingCandidate=Join-Path $project ('.build/server-candidate/run-'+[guid]::NewGuid().ToString('N'))
$entryRejections=@(
    @{pattern='explicit -RealPlayer';call={ & (Join-Path $PSScriptRoot 'stage_isolated_capture.ps1') -CandidateRoot $missingCandidate -PlayerName 'Example' }},
    @{pattern='requires a nonzero';call={ & (Join-Path $PSScriptRoot 'stage_isolated_capture.ps1') -CandidateRoot $missingCandidate -RealPlayer -PlayerName 'Example' }},
    @{pattern='requires a nonzero';call={ & (Join-Path $PSScriptRoot 'check_capture_isolation.ps1') -SandboxRoot $missingRoot -RealPlayer -PlayerName 'Example' }},
    @{pattern='requires a nonzero';call={ & (Join-Path $PSScriptRoot 'prepare_capture_executable.ps1') -SandboxRoot $missingRoot -RealPlayer -PlayerName 'Example' }},
    @{pattern='requires explicit -RealPlayer';call={ & (Join-Path $PSScriptRoot 'run_isolated_capture.ps1') -SandboxRoot $missingRoot -PlayerWindowSeconds 30 }},
    @{pattern='requires a nonzero';call={ & (Join-Path $PSScriptRoot 'run_isolated_capture.ps1') -SandboxRoot $missingRoot -RealPlayer -PlayerName 'Example' }},
    @{pattern='requires a nonzero';call={ & (Join-Path $PSScriptRoot 'verify_real_player_capture.ps1') -SandboxRoot $missingRoot -PlayerName 'Example' -PlayerXuid '0' }}
)
foreach ($case in $entryRejections) {
    $rejected=$false
    try { & $case.call | Out-Null } catch {
        if ($_.Exception.Message -notmatch $case.pattern) { throw }
        $rejected=$true
    }
    if (-not $rejected) { throw 'Entrypoint accepted invalid capture mode/identity' }
}
if ([IO.Directory]::Exists($missingRoot) -or [IO.Directory]::Exists($missingCandidate)) { throw 'Invalid input unexpectedly created an owned run' }
"One-player capture policy PASS ($($rejections.Count) access + $($boundaryRejections.Count) boundary + $($entryRejections.Count) entrypoint rejections, $($cases.Count) window/log cases; metadata ONLY, no gameplay proof)"
