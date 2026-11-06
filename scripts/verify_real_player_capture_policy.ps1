$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'real_player_capture_policy.ps1')
# In-memory policy examples ONLY. Never persisted, sent to BDS or claimed as
# real-client/game evidence. No SQLite DB, world or runtime is fabricated here.
foreach($value in @(30,300,600,3600,[long]3600)) {
    if(-not (Test-BdsRealPlayerWindow $value)){throw 'Authorized player window rejected'}
}
foreach($value in @($null,$true,$false,'600',600.0,29,3601,-1)) {
    if(Test-BdsRealPlayerWindow $value){throw 'Invalid player window accepted'}
}
$runner=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'run_isolated_capture.ps1')
if($runner -notmatch '\[ValidateRange\(30,3600\)\]\[int\]\$PlayerWindowSeconds') {
    throw 'Player window policy drifted from runner; review both bounds'
}
$join=[pscustomobject]@{event_id=1;occurred_at_ms=100;event_kind='session_start';actor_kind='player';actor_id='uuid64:1:2';actor_name='Example';session_id='session-example';cause='levilamina_player_event'}
$leave=$join.psobject.Copy(); $leave.event_id=2; $leave.occurred_at_ms=200; $leave.event_kind='session_end'
$session=[pscustomobject]@{session_id='session-example';player_id='uuid64:1:2';started_at_ms=100;ended_at_ms=200;duration_ms=100}
$positive=Test-BdsRealPlayerRows @($join,$leave) @($session) 'Example' 50 250
if (-not $positive.database_pair_verified) { throw 'Valid policy example rejected' }
$maxJoin=$join.psobject.Copy(); $maxLeave=$leave.psobject.Copy(); $maxSession=$session.psobject.Copy()
$maxJoin.actor_id='uuid64:18446744073709551615:18446744073709551615'
$maxLeave.actor_id=$maxJoin.actor_id; $maxSession.player_id=$maxJoin.actor_id
if (-not (Test-BdsRealPlayerRows @($maxJoin,$maxLeave) @($maxSession) 'Example' 50 250).database_pair_verified) {
    throw 'Valid UInt64 UUID components rejected'
}
$cases=@(
    { Test-BdsRealPlayerRows @() @() 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join) @($session) 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join,$join,$leave) @($session) 'Example' 50 250 },
    { $x=$leave.psobject.Copy();$x.actor_id='uuid64:9:9';Test-BdsRealPlayerRows @($join,$x) @($session) 'Example' 50 250 },
    { $x=$join.psobject.Copy();$x.actor_id='uuid64:0:0';Test-BdsRealPlayerRows @($x,$leave) @($session) 'Example' 50 250 },
    { $x=$leave.psobject.Copy();$x.session_id='another';Test-BdsRealPlayerRows @($join,$x) @($session) 'Example' 50 250 },
    { $x=$leave.psobject.Copy();$x.event_id=1;Test-BdsRealPlayerRows @($join,$x) @($session) 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join,$leave) @($session) 'Example' 150 250 },
    { Test-BdsRealPlayerRows @($join,$leave) @($session) 'Example' 50 150 },
    { $x=$session.psobject.Copy();$x.ended_at_ms=$null;Test-BdsRealPlayerRows @($join,$leave) @($x) 'Example' 50 250 },
    { $x=$session.psobject.Copy();$x.duration_ms=99;Test-BdsRealPlayerRows @($join,$leave) @($x) 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join,$leave) @() 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join,$leave) @($session) 'NotExample' 50 250 },
    { $x=$join.psobject.Copy();$x.actor_id='uuid64:18446744073709551616:2';Test-BdsRealPlayerRows @($x,$leave) @($session) 'Example' 50 250 },
    { $x=$join.psobject.Copy();$x.actor_id=$null;Test-BdsRealPlayerRows @($x,$leave) @($session) 'Example' 50 250 },
    { $x=$join.psobject.Copy();$x.cause='fixture';Test-BdsRealPlayerRows @($x,$leave) @($session) 'Example' 50 250 },
    { $x=$leave.psobject.Copy();$x.actor_kind='unknown';Test-BdsRealPlayerRows @($join,$x) @($session) 'Example' 50 250 },
    { $x=$leave.psobject.Copy();$x.occurred_at_ms=99;Test-BdsRealPlayerRows @($join,$x) @($session) 'Example' 50 250 },
    { $x=$session.psobject.Copy();$x.duration_ms=$null;Test-BdsRealPlayerRows @($join,$leave) @($x) 'Example' 50 250 },
    { Test-BdsRealPlayerRows @($join,$leave) @($session,$session) 'Example' 50 250 }
)
foreach ($case in $cases) {
    if ((& $case).database_pair_verified) { throw 'Invalid real-player observation example accepted' }
}
"Real-player observation policy PASS ($($cases.Count) negative pair cases,5 valid/8 invalid windows; not game/event-to-DB proof)"
