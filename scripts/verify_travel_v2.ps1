# Deterministic native fixtures only: no BDS, no live DB, no deployment.
param([switch]$WorldTick,[switch]$FishBucket,[switch]$RaidEffect,[switch]$Absorption,[switch]$Rest,[switch]$Resistance,[switch]$ShieldEvidence,[switch]$HealthLoss,[switch]$TypeLabels,[switch]$BlockRecords,[switch]$BrokenRecords)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$travelProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
foreach($name in @('BEDROCKINSIGHT_CLANGCL_ROOT','BEDROCKINSIGHT_SQLITE_ROOT')) {
    if(-not [Environment]::GetEnvironmentVariable($name)){throw "$name is required"}
}
$travelCompiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$travelSqlite=$env:BEDROCKINSIGHT_SQLITE_ROOT
$travelBuild=Join-Path $travelProject ('.build\travel-v2-tests\run-'+[guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $travelBuild
New-Item -ItemType Directory -Path $travelBuild | Out-Null
$travelTestNames=@('travel_window_tests','parity_observation_tests','stats_reader_tests','remote_api_tests')
if($WorldTick){$travelTestNames+= 'world_tick_tests'}
if($FishBucket){$travelTestNames+= 'fish_bucket_tests'}
if($RaidEffect){$travelTestNames+= 'raid_effect_tests'}
if($Absorption){$travelTestNames+= 'absorption_tests'}
if($Rest){$travelTestNames+= 'rest_tests'}
if($Resistance){$travelTestNames+= 'resistance_tests'}
if($ShieldEvidence){$travelTestNames+= 'shield_evidence_tests'}
if($HealthLoss){$travelTestNames+= 'health_loss_tests'}
if($TypeLabels){$travelTestNames+= 'type_labels_tests'}
if($BlockRecords){$travelTestNames+= @('block_observation_tests','block_record_tests')}
if($BrokenRecords){$travelTestNames+= @('hurt_observation_tests','broken_record_tests')}
function Get-TravelHashes {
    $result=[ordered]@{}
    $inputs=@(Get-ChildItem (Join-Path $travelProject 'src') -File -Recurse | Where-Object {$_.Extension -in '.cpp','.hpp','.h'})
    $inputs+=Get-Item $PSCommandPath
    $inputs+=Get-Item (Join-Path $travelProject 'companion/lang/ja_JP.json')
    $inputs+=@(Get-ChildItem (Join-Path $travelProject 'companion'),(Join-Path $travelProject 'tests') -Recurse -File | Where-Object {$_.Extension -in '.py','.js','.cjs','.html','.css'})
    $inputs+=Get-Item (Join-Path $PSScriptRoot 'invoke_logged_native.ps1'),(Join-Path $PSScriptRoot 'validate_server_release.ps1')
    $inputs+=@(Get-ChildItem (Join-Path $travelProject 'tests') -File | Where-Object {$_.Extension -in '.h','.hpp'})
    foreach($test in $travelTestNames) {
        $inputs+=Get-Item (Join-Path $travelProject "tests/$test.cpp")
    }
    foreach($inputFile in ($inputs | Sort-Object FullName)) {
        $result[$inputFile.FullName.Substring($travelProject.Length+1)]=(Get-FileHash -LiteralPath $inputFile.FullName).Hash
    }
    return $result
}
$receipt=[ordered]@{kind='travel-v2-native-fixtures-v1';passed=$false;bds_started=$false;native_dll_built=$false;
    full_parity_accepted=$false;scope='Synthetic model -> native event builder -> StorageWriter -> SQLite -> native/HTTP readers -> Japanese JS DOM';
    before=(Get-TravelHashes);after=$null;error=$null}
$travelTemp=$env:TEMP;$travelTmp=$env:TMP
$travelPythonPath=$env:PYTHONPATH
Write-Output $travelBuild
Push-Location $travelBuild
try {
    $flags=@('/nologo','/EHsc','/std:c++20','/MD','/W4','/WX','/utf-8',"/I$travelProject/src","/I$travelSqlite/include")
    $objects=@();$core=@('/c')+$flags
    foreach($name in @('stats_reader','storage','event','sqlite_api','rollback','admin_api','remote_api','json','type_labels')) {
        $core+=Join-Path $travelProject "src/bedrock_insight/core/$name.cpp"
        $objects+=Join-Path $travelBuild "$name.obj"
    }
    Invoke-BdsLoggedNative $travelCompiler $core (Join-Path $travelBuild 'core-compile.log')
    foreach($test in $travelTestNames) {
        $temp=Join-Path $travelBuild "tmp-$test"
        New-Item -ItemType Directory -Path $temp | Out-Null
        $env:TEMP=$temp;$env:TMP=$temp
        $exe=Join-Path $travelBuild "$test.exe"
        $travelArguments=$flags+@((Join-Path $travelProject "tests/$test.cpp"))+ $objects + @("$travelSqlite/lib/sqlite3.lib","/Fe:$exe")
        Invoke-BdsLoggedNative $travelCompiler $travelArguments (Join-Path $travelBuild "$test-compile.log")
        Invoke-BdsLoggedNative $exe @() (Join-Path $travelBuild "$test.log")
        Get-Content (Join-Path $travelBuild "$test.log") -Tail 2
    }
    $env:PYTHONPATH=(Join-Path $travelProject 'tests')+';'+$travelProject
    $readback='from pathlib import Path; import sys; from test_travel_v2 import http_readback; http_readback(Path(sys.argv[1])); print("Native writer -> authenticated HTTP -> Japanese DOM PASS; synthetic only")'
    Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$readback,
        (Join-Path $travelBuild 'tmp-travel_window_tests/travel_v2_fixture.sqlite3')) (Join-Path $travelBuild 'http-dom-readback.log')
    Get-Content (Join-Path $travelBuild 'http-dom-readback.log') -Tail 1
    if($WorldTick) {
        $worldReadback='from pathlib import Path; import sys; from test_world_tick import http_readback; http_readback(Path(sys.argv[1])); print("Native world snapshot -> authenticated HTTP -> Japanese DOM PASS; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$worldReadback,
            (Join-Path $travelBuild 'tmp-world_tick_tests/world_tick_fixture.sqlite3')) (Join-Path $travelBuild 'world-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'world-http-dom-readback.log') -Tail 1
    }
    if($FishBucket) {
        $fishReadback='from pathlib import Path; import sys; from test_fish_bucket import http_readback; http_readback(Path(sys.argv[1])); print("Native fish builder -> authenticated HTTP -> Japanese DOM PASS; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$fishReadback,
            (Join-Path $travelBuild 'tmp-fish_bucket_tests/fish_bucket_fixture.sqlite3')) (Join-Path $travelBuild 'fish-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'fish-http-dom-readback.log') -Tail 1
    }
    if($RaidEffect) {
        $raidReadback='from pathlib import Path; import sys; from test_raid_effect import http_readback; http_readback(Path(sys.argv[1])); print("Native raid-effect builder -> authenticated HTTP -> Japanese DOM PASS; synthetic only, not raid proof")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$raidReadback,
            (Join-Path $travelBuild 'tmp-raid_effect_tests/raid_effect_fixture.sqlite3')) (Join-Path $travelBuild 'raid-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'raid-http-dom-readback.log') -Tail 1
    }
    if($Absorption) {
        $absReadback='from pathlib import Path; import sys; from test_absorption import http_readback; http_readback(Path(sys.argv[1])); print("Native absorption builder -> authenticated HTTP -> Japanese DOM PASS; synthetic only, not engine proof")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$absReadback,
            (Join-Path $travelBuild 'tmp-absorption_tests/absorption_fixture.sqlite3')) (Join-Path $travelBuild 'absorption-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'absorption-http-dom-readback.log') -Tail 1
    }
    if($Rest) {
        $restReadback='from pathlib import Path; import sys; from test_rest import http_readback; http_readback(Path(sys.argv[1])); print("Native rest snapshot -> authenticated HTTP -> Japanese DOM PASS; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$restReadback,
            (Join-Path $travelBuild 'tmp-rest_tests/rest_fixture.sqlite3')) (Join-Path $travelBuild 'rest-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'rest-http-dom-readback.log') -Tail 1
    }
    if($Resistance) {
        $resistReadback='from pathlib import Path; import sys; from test_resistance import http_readback; http_readback(Path(sys.argv[1])); print("Native resistance builder -> authenticated HTTP -> Japanese DOM PASS; synthetic formula only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$resistReadback,
            (Join-Path $travelBuild 'tmp-resistance_tests/resistance_fixture.sqlite3')) (Join-Path $travelBuild 'resistance-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'resistance-http-dom-readback.log') -Tail 1
    }
    if($ShieldEvidence) {
        $shieldReadback='from pathlib import Path; import sys; from test_shield_evidence import http_readback; http_readback(Path(sys.argv[1])); print("Native shield raw evidence -> private inspector -> authenticated Japanese JS remains UNAVAILABLE; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$shieldReadback,
            (Join-Path $travelBuild 'tmp-shield_evidence_tests/shield_evidence_fixture.sqlite3')) (Join-Path $travelBuild 'shield-evidence-readback.log')
        Get-Content (Join-Path $travelBuild 'shield-evidence-readback.log') -Tail 1
    }
    if($HealthLoss) {
        $healthReadback='from pathlib import Path; import sys; from test_health_loss import http_readback; http_readback(Path(sys.argv[1])); print("Native HEALTH delta -> SQLite -> authenticated HTTP -> Japanese JS PASS; no notification/absorption mixing; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$healthReadback,
            (Join-Path $travelBuild 'tmp-health_loss_tests/health_loss_fixture.sqlite3')) (Join-Path $travelBuild 'health-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'health-http-dom-readback.log') -Tail 1
    }
    if($BlockRecords) {
        $blockReadback='from pathlib import Path; import sys; from test_block_records import http_readback; http_readback(Path(sys.argv[1])); print("Native v2 block builder -> strict native/C API + authenticated HTTP/Japanese JS agree; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$blockReadback,
            (Join-Path $travelBuild 'tmp-block_record_tests/block_record_fixture.sqlite3')) (Join-Path $travelBuild 'block-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'block-http-dom-readback.log') -Tail 1
    }
    if($BrokenRecords) {
        $brokenReadback='from pathlib import Path; import sys; from test_broken_records import http_readback; http_readback(Path(sys.argv[1])); print("Native broken v2 builder -> SQLite/native/C API + authenticated HTTP/Japanese JS agree; synthetic only")'
        Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$brokenReadback,
            (Join-Path $travelBuild 'tmp-broken_record_tests/broken_record_fixture.sqlite3')) (Join-Path $travelBuild 'broken-http-dom-readback.log')
        Get-Content (Join-Path $travelBuild 'broken-http-dom-readback.log') -Tail 1
    }
    $receipt.after=Get-TravelHashes
    if((ConvertTo-Json $receipt.before -Compress) -cne (ConvertTo-Json $receipt.after -Compress)){throw 'Native source drift during fixture tests'}
    $receipt.passed=$true
} catch {$receipt.error=$_.Exception.Message;throw}
finally {
    $receipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $travelBuild 'fixture-evidence.json') -Encoding utf8
    $env:TEMP=$travelTemp;$env:TMP=$travelTmp;$env:PYTHONPATH=$travelPythonPath;Pop-Location
}
