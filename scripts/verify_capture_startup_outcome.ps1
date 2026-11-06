$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'capture_startup_outcome.ps1')
# Pure observation fixtures; no server, executable, filesystem mutation or network.
$cases=@(
    @{reason='startup_deadline';error=$true;stop=$true;deadline=$true;independent=$false;censored=$true},
    @{reason='process_exited';error=$true;stop=$false;deadline=$false;independent=$true;censored=$false},
    @{reason='runner_error';error=$true;stop=$true;deadline=$false;independent=$false;censored=$true},
    @{reason='ready_hold_completed';error=$false;stop=$true;deadline=$false;independent=$false;censored=$false},
    @{reason='startup_deadline';error=$false;stop=$true;deadline=$true;independent=$false;censored=$false},
    @{reason='process_exited';error=$false;stop=$false;deadline=$false;independent=$false;censored=$false},
    @{reason='runner_error';error=$true;stop=$false;deadline=$false;independent=$false;censored=$false},
    @{reason='player_roundtrip_observed';error=$false;stop=$true;deadline=$false;independent=$false;censored=$false},
    @{reason='player_window_timeout';error=$true;stop=$true;deadline=$false;independent=$false;censored=$true},
    @{reason='player_identity_mismatch';error=$false;stop=$true;deadline=$false;independent=$false;censored=$false},
    @{reason='player_sequence_invalid';error=$false;stop=$true;deadline=$false;independent=$false;censored=$false}
)
foreach ($case in $cases) {
    $value=Get-BdsCaptureStartupOutcome $case.reason $case.error $case.stop
    if ($value.startup_deadline_exceeded -ne $case.deadline -or
        $value.service_error_independent_of_test_stop -ne $case.independent -or
        $value.service_error_may_be_stop_censored -ne $case.censored) { throw 'Startup observation classification mismatch' }
}
$rejected=$false
try { Get-BdsCaptureStartupOutcome 'invented_network_cause' $true $true | Out-Null } catch { $rejected=$true }
if (-not $rejected) { throw 'Unknown startup outcome accepted' }
"Capture startup outcome PASS ($($cases.Count) observation cases + invalid reason rejection; no BDS/network)"
