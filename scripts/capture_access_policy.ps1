# Pure access/window policy. No file writes, process control, network or game events.
function New-BdsCaptureAccessPolicy([bool]$RealPlayer, [string]$PlayerName, [string]$PlayerXuid) {
    if (-not $RealPlayer) {
        if ($PlayerName -or $PlayerXuid) { throw 'Player identity requires explicit -RealPlayer mode' }
        return [pscustomobject]@{mode='empty-smoke';player_name=$null;player_xuid=$null}
    }
    if (-not $PlayerName -or $PlayerName -cnotmatch '^[^,\x00-\x1f\x7f]{1,64}$' -or $PlayerName.Trim() -cne $PlayerName) {
        throw 'Real-player mode requires an exact nonblank gamertag without commas/control characters'
    }
    $number=[UInt64]0
    if ($PlayerXuid -cnotmatch '^[1-9][0-9]{0,19}$' -or -not [UInt64]::TryParse($PlayerXuid,[ref]$number)) {
        throw 'Real-player mode requires a nonzero canonical UInt64 Xbox XUID string'
    }
    [pscustomobject]@{mode='one-real-player';player_name=$PlayerName;player_xuid=$PlayerXuid}
}

function Assert-BdsCaptureAccessPolicy($Policy, $Expected) {
    if ($null -eq $Policy -or @($Policy.psobject.Properties).Count -ne 3 -or
        @($Policy.psobject.Properties.Name | Where-Object { $_ -cnotin @('mode','player_name','player_xuid') }).Count -ne 0 -or
        $Policy.mode -isnot [string] -or
        ($Expected.mode -ceq 'one-real-player' -and ($Policy.player_name -isnot [string] -or $Policy.player_xuid -isnot [string])) -or
        $Policy.mode -cne $Expected.mode -or $Policy.player_name -cne $Expected.player_name -or
        $Policy.player_xuid -cne $Expected.player_xuid) { throw 'Staged access policy differs from explicitly requested mode/identity' }
}

function Get-BdsCaptureAllowlistJson($Access) {
    if ($Access.mode -ceq 'empty-smoke') {
        Assert-BdsCaptureAccessPolicy $Access (New-BdsCaptureAccessPolicy $false '' '')
        return '[]'
    }
    $checked=New-BdsCaptureAccessPolicy $true $Access.player_name $Access.player_xuid
    Assert-BdsCaptureAccessPolicy $Access $checked
    # Fixed serialization, including false bypass flag; no duplicate JSON keys,
    # extra account or name-only entry can be supplied through a raw JSON input.
    ConvertTo-Json -InputObject @([ordered]@{name=$checked.player_name;xuid=$checked.player_xuid;ignoresPlayerLimit=$false}) -Compress
}

function Assert-BdsCapturePlayerAllowlist([string]$Text, $Access) {
    $checked=New-BdsCaptureAccessPolicy $true $Access.player_name $Access.player_xuid
    Assert-BdsCaptureAccessPolicy $Access $checked
    if ($Text.Trim() -cne (Get-BdsCaptureAllowlistJson $checked)) {
        throw 'Real-player allowlist must be exactly the generated single name/XUID entry with bypass disabled'
    }
}

function Get-BdsCapturePlayerLogPair([string[]]$Lines, $Access) {
    $checked=New-BdsCaptureAccessPolicy $true $Access.player_name $Access.player_xuid
    Assert-BdsCaptureAccessPolicy $Access $checked
    $joins=0; $leaves=0; $unexpected=$false; $ordered=$false
    foreach ($line in $Lines) {
        # Exact LL26.60 English server-prefix format, not chat text or a
        # substring containing a player-controlled name. Unknown formats fail closed.
        if ($line -cmatch '^\d{2}:\d{2}:\d{2}\.\d{3} IFO \[Server\] Player (connected|disconnected): ([^,\r\n]+), xuid: ([0-9]+)(?:,.*)?$') {
            if ($Matches[2] -cne $checked.player_name -or $Matches[3] -cne $checked.player_xuid) { $unexpected=$true; continue }
            if ($Matches[1] -ceq 'connected') { ++$joins }
            else { ++$leaves; $ordered=($joins -eq 1 -and $leaves -eq 1) }
        }
    }
    [pscustomobject]@{connect_count=$joins;disconnect_count=$leaves;unexpected_identity=$unexpected;
        pair_observed=($joins -eq 1 -and $leaves -eq 1 -and $ordered -and -not $unexpected)}
}

function Get-BdsCaptureWindowDecision($Access, [long]$ReadyElapsedMs, [int]$WindowSeconds, [string[]]$Lines) {
    if ($ReadyElapsedMs -lt 0 -or $WindowSeconds -lt 30 -or $WindowSeconds -gt 3600) { throw 'Invalid bounded capture-window observation' }
    $reason=$null; $pair=$null
    if ($Access.mode -ceq 'empty-smoke') {
        Assert-BdsCaptureAccessPolicy $Access (New-BdsCaptureAccessPolicy $false '' '')
        if ($ReadyElapsedMs -ge 3000) { $reason='ready_hold_completed' }
    } else {
        $pair=Get-BdsCapturePlayerLogPair $Lines $Access
        if ($pair.unexpected_identity) { $reason='player_identity_mismatch' }
        elseif ($pair.connect_count -gt 1 -or $pair.disconnect_count -gt 1 -or ($pair.disconnect_count -eq 1 -and -not $pair.pair_observed)) { $reason='player_sequence_invalid' }
        elseif ($ReadyElapsedMs -ge ([long]$WindowSeconds * 1000)) { $reason='player_window_timeout' }
        elseif ($pair.pair_observed) { $reason='player_roundtrip_observed' }
    }
    [pscustomobject]@{stop=($null -ne $reason);stop_reason=$reason;player_logs=$pair}
}
