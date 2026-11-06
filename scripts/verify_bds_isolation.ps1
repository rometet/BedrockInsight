$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
$profilePath = Join-Path $projectRoot 'packaging/bds-profile.json'
$profile = Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
$profileHash = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
$tooth = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/tooth.json') | ConvertFrom-Json
$modManifest = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'manifest.json') | ConvertFrom-Json
$runId = [guid]::NewGuid().ToString('N')
$candidate = Join-Path $projectRoot ".build/isolated-bds/run-$runId"
# In-memory metadata fixtures only: no fake EXE/DLL or release evidence is written.
$facts = [pscustomobject]@{
    sandbox_root = $candidate
    build_evidence = [pscustomobject]@{
        compile_probe=$false;sdk_is_server=$true;sdk_version_matches_2660=$true
        dll_sha256=('a' * 64);bds_profile_sha256=$profileHash;bds_profile=$profile
        sdk_dependency_inputs=[pscustomobject]@{
            dependencies=@([pscustomobject]@{name='bedrockdata';version='v26.60.6-server.5'},
                [pscustomobject]@{name='prelink';version='v0.8.8'})
            defines=@('ENTT_PACKED_PAGE=128','ENTT_SPARSE_PAGE=4096')
        }
    }
    mod_manifest=$modManifest;actual_dll_sha256=('a' * 64)
    allowlist_empty=$true;permissions_empty=$true;world_directory_exists=$false
    mod_directories=@('BedrockInsight','LeviLamina');bound_ports=@(19132)
    running_bds_paths=@('C:\unrelated-server\bedrock_server.exe')
    server_properties=@"
server-name=BedrockInsight isolated test
transport=nethernet
server-port=59162
server-udp-ports=59162
server-portv6=59163
enable-lan-visibility=false
allow-list=true
online-mode=true
max-players=1
default-player-permission-level=visitor
allow-cheats=false
level-name=insight-isolated-$runId
"@
}
$serialized = $facts | ConvertTo-Json -Depth 10
function Check-Candidate($Value) { Test-BdsIsolationCandidate $Value $profile $tooth $profileHash $projectRoot }
$positive = Check-Candidate $facts
if (-not $positive.candidate_policy_passed -or $positive.launch_authorized -or $positive.bds_load_verified) {
    throw 'Policy must not authorize launch or assert BDS acceptance'
}
$cases = @(
    @{pattern='Isolation root';change={param($d) $d.sandbox_root=$projectRoot}},
    @{pattern='Isolation root';change={param($d) $d.sandbox_root='C:\outside\run-' + $runId}},
    @{pattern='Isolation root';change={param($d) $d.sandbox_root=$candidate + '\child'}},
    @{pattern='Release evidence';change={param($d) $d.build_evidence.compile_probe=$true}},
    @{pattern='evidence flag';change={param($d) $d.build_evidence.sdk_is_server='true'}},
    @{pattern='Release evidence';change={param($d) $d.build_evidence.bds_profile_sha256='b' * 64}},
    @{pattern='DLL hash mismatch';change={param($d) $d.actual_dll_sha256='b' * 64}},
    @{pattern='SDK dependency mismatch';change={param($d) $d.build_evidence.sdk_dependency_inputs.dependencies[0].version='v26.51.1-server.6'}},
    @{pattern='allowlist_empty';change={param($d) $d.allowlist_empty=$false}},
    @{pattern='permissions_empty';change={param($d) $d.permissions_empty=$false}},
    @{pattern='fresh world';change={param($d) $d.world_directory_exists=$true}},
    @{pattern='Unexpected mod';change={param($d) $d.mod_directories += 'UnrelatedMod'}},
    @{pattern='Unsafe isolation property';change={param($d) $d.server_properties=$d.server_properties.Replace('allow-list=true','allow-list=false')}},
    @{pattern='Unsafe isolation property: transport';change={param($d) $d.server_properties=$d.server_properties.Replace('transport=nethernet','transport=raknet')}},
    @{pattern='Unsafe isolation property: server-udp-ports';change={param($d) $d.server_properties=$d.server_properties.Replace('server-udp-ports=59162','server-udp-ports=19132')}},
    @{pattern='Duplicate';change={param($d) $d.server_properties += "`nserver-port=59164"}},
    @{pattern='high ports';change={param($d) $d.server_properties=$d.server_properties.Replace('59162','19132')}},
    @{pattern='distinct';change={param($d) $d.server_properties=$d.server_properties.Replace('59163','59162')}},
    @{pattern='already in use';change={param($d) $d.bound_ports += 59162}},
    @{pattern='running BDS';change={param($d) $d.running_bds_paths=@($candidate + '\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@($null)}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('\\?\C:\outside\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('\\server\share\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:\OUTSID~1\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:\outside\..\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:\outside.\bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:\outside \bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@('C:/outside/bedrock_server.exe')}},
    @{pattern='Cannot resolve';change={param($d) $d.running_bds_paths=@(42)}},
    @{pattern='Unreviewed';change={param($d) $d.server_properties += "`nunknown-path=C:\outside"}}
)
foreach ($case in $cases) {
    $value = $serialized | ConvertFrom-Json
    & $case.change $value
    $caught = $false
    try { Check-Candidate $value | Out-Null }
    catch { if ($_.Exception.Message -notmatch $case.pattern) { throw }; $caught=$true }
    if (-not $caught) { throw "Expected isolation policy rejection: $($case.pattern)" }
}
# MainModule errors must never be mistaken for an empty process inventory.
# These are metadata objects only, not EXEs or launched test processes.
$processCases = @(
    [pscustomobject]@{ MainModule=$null },
    [pscustomobject]@{ MainModule=[pscustomobject]@{FileName=$null} },
    [pscustomobject]@{ MainModule=[pscustomobject]@{FileName='C:\outside\wrong.exe'} }
)
$denied = [pscustomobject]@{}
$denied | Add-Member ScriptProperty MainModule { throw [UnauthorizedAccessException]::new('test access denied') }
$exited = [pscustomobject]@{}
$exited | Add-Member ScriptProperty MainModule { throw [InvalidOperationException]::new('test process exited') }
$processCases += @($denied,$exited)
foreach ($processCase in $processCases) {
    $caught = $false
    try { Get-BdsProcessExecutablePath $processCase 'bedrock_server' | Out-Null }
    catch { if ($_.Exception.Message -notmatch 'Cannot resolve') { throw }; $caught=$true }
    if (-not $caught) { throw 'Expected inaccessible/exited/unresolved process rejection' }
}
foreach ($name in @('bedrock_server','bedrock_server_mod')) {
    $path = "C:\outside\$name.exe"
    $resolved = Get-BdsProcessExecutablePath ([pscustomobject]@{MainModule=[pscustomobject]@{FileName=$path}}) $name
    if ($resolved -cne $path) { throw 'Expected unmodified absolute process path' }
}
# Real directory/junction rejection, with both link and target in the project.
$fixture = Join-Path $projectRoot ('.build/isolation-policy-tests/' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $fixture
New-Item -ItemType Directory -Path (Join-Path $fixture 'target') -Force | Out-Null
Assert-BdsUnredirectedTree (Join-Path $fixture 'target')
$junction = Join-Path $fixture 'redirect'
New-Item -ItemType Junction -Path $junction -Value (Join-Path $fixture 'target') | Out-Null
$caught = $false
try { Assert-BdsUnredirectedTree $fixture }
catch { if ($_.Exception.Message -notmatch 'Redirected isolation path') { throw }; $caught=$true }
if (-not $caught) { throw 'Expected real junction rejection' }
$caught = $false
try { Assert-BdsUnredirectedAncestors (Join-Path $junction 'missing-child') }
catch { if ($_.Exception.Message -notmatch 'Redirected isolation path') { throw }; $caught=$true }
if (-not $caught) { throw 'Expected redirected ancestor rejection before missing child access' }
"BDS isolation candidate policy: PASS ($($cases.Count) metadata + $($processCases.Count) process + 2 junction rejections; no launch/EXE/DLL fixtures)"
