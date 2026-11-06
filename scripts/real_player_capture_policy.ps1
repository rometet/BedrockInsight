# Post-exit observations only. This cannot manufacture a player or world event.
function Test-BdsRealPlayerWindow($Value) {
    # Match the already-authorized runner's 30..3600-second bound. Reject
    # coercible strings, floating point and booleans from edited evidence.
    return (($Value -is [int] -or $Value -is [long]) -and $Value -ge 30 -and $Value -le 3600)
}

function Test-BdsRealPlayerRows($Events, $Sessions, [string]$PlayerName, [long]$StartMs, [long]$EndMs) {
    $issues=[Collections.Generic.List[string]]::new()
    $eventsForPlayer=@($Events | Where-Object { $_.actor_name -ceq $PlayerName -and $_.event_kind -cin @('session_start','session_end') })
    $joins=@($eventsForPlayer | Where-Object { $_.event_kind -ceq 'session_start' })
    $leaves=@($eventsForPlayer | Where-Object { $_.event_kind -ceq 'session_end' })
    if ($joins.Count -ne 1 -or $leaves.Count -ne 1) { $issues.Add('Expected exactly one session_start and one session_end for the specified real player') }
    $session=$null; $actor=$null; $sessionId=$null
    if ($joins.Count -eq 1 -and $leaves.Count -eq 1) {
        $join=$joins[0]; $leave=$leaves[0]; $actor=$join.actor_id; $sessionId=$join.session_id
        $validUuid=$false
        if ($actor -cmatch '^uuid64:([0-9]{1,20}):([0-9]{1,20})$') {
            $first=[UInt64]0; $second=[UInt64]0
            $validUuid=[UInt64]::TryParse($Matches[1],[ref]$first) -and [UInt64]::TryParse($Matches[2],[ref]$second) -and ($first -ne 0 -or $second -ne 0)
        }
        if (-not $validUuid -or $join.actor_kind -cne 'player' -or $leave.actor_kind -cne 'player' -or
            $leave.actor_id -cne $actor -or -not $sessionId -or $leave.session_id -cne $sessionId -or
            $join.cause -cne 'levilamina_player_event' -or $leave.cause -cne 'levilamina_player_event') {
            $issues.Add('Session pair lacks one consistent native player UUID/session/cause')
        }
        if ($StartMs -gt $EndMs -or $join.occurred_at_ms -lt $StartMs -or $leave.occurred_at_ms -gt $EndMs -or
            $join.occurred_at_ms -gt $leave.occurred_at_ms -or $join.event_id -le 0 -or $join.event_id -ge $leave.event_id) {
            $issues.Add('Session pair has invalid event ordering or falls outside the recorded runtime window')
        }
        $matching=@($Sessions | Where-Object { $_.session_id -ceq $sessionId -and $_.player_id -ceq $actor })
        if ($matching.Count -ne 1) { $issues.Add('Expected one corresponding persisted session row') }
        else {
            $session=$matching[0]
            if ($null -eq $session.ended_at_ms -or $null -eq $session.duration_ms -or
                $session.started_at_ms -ne $join.occurred_at_ms -or $session.ended_at_ms -ne $leave.occurred_at_ms -or
                $session.duration_ms -ne ($leave.occurred_at_ms - $join.occurred_at_ms)) {
                $issues.Add('Persisted session is open or does not match the raw join/leave times and duration')
            }
        }
    }
    [pscustomobject]@{database_pair_verified=($issues.Count -eq 0);issues=$issues.ToArray();
        join_rows=$joins.Count;leave_rows=$leaves.Count;actor_id=$actor;session_id=$sessionId;session=$session}
}
