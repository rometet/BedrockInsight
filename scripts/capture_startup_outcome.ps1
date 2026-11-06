# Classify observations, never infer network root cause from stop-censored logs.
function Get-BdsCaptureStartupOutcome {
    param([ValidateSet('ready_hold_completed','startup_deadline','process_exited','runner_error',
        'player_roundtrip_observed','player_window_timeout','player_identity_mismatch','player_sequence_invalid')][string]$StopReason,
          [bool]$ServiceErrorInFinalLogs, [bool]$StopCommandIssued)
    [pscustomobject]@{
        stop_reason=$StopReason
        startup_deadline_exceeded=($StopReason -ceq 'startup_deadline')
        service_error_in_final_logs=$ServiceErrorInFinalLogs
        service_error_independent_of_test_stop=($ServiceErrorInFinalLogs -and -not $StopCommandIssued -and $StopReason -ceq 'process_exited')
        service_error_may_be_stop_censored=($ServiceErrorInFinalLogs -and $StopCommandIssued)
        limits='Observation classification only. A service error after an operator stop does not establish unreachable services, proxy causation or an intrinsic BDS timeout.'
    }
}
