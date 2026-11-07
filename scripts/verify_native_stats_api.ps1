# Isolated core DLL + genuine C caller; NEVER loads an engine/LL/BDS mod.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$apiProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
foreach($apiName in @('BEDROCKINSIGHT_CLANGCL_ROOT','BEDROCKINSIGHT_SQLITE_ROOT')) {
    if(-not [Environment]::GetEnvironmentVariable($apiName)){throw "$apiName is required"}
}
$apiBuild=Join-Path $apiProject ('.build\c-api-tests\run-'+[guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $apiBuild
New-Item -ItemType Directory -Path $apiBuild | Out-Null
$apiCompiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$apiInputs=@('src/bedrock_insight/core/remote_api.cpp','src/bedrock_insight/core/stats_reader.cpp',
    'src/bedrock_insight/core/sqlite_api.cpp','tests/remote_api_fixture.cpp','tests/remote_api_dynamic_tests.c','tests/block_record_fixture.hpp',
    'scripts/verify_native_stats_api.ps1','scripts/invoke_logged_native.ps1','scripts/validate_bds_isolation.ps1')
$apiInputs+=@(Get-ChildItem (Join-Path $apiProject 'src/bedrock_insight/core') -File |
    Where-Object {$_.Extension -in '.h','.hpp'} | ForEach-Object {$_.FullName.Substring($apiProject.Length+1)})
$apiInputs+=@('src/bedrock_insight/core/json.cpp','src/bedrock_insight/core/type_labels.cpp','companion/lang/ja_JP.json')
function Get-ApiHashes {
    $apiHashes=[ordered]@{}
    foreach($apiInput in ($apiInputs|Sort-Object -Unique)) {
        $apiHashes[$apiInput]=(Get-FileHash -LiteralPath (Join-Path $apiProject $apiInput)).Hash
    }
    return $apiHashes
}
$apiReceipt=[ordered]@{kind='native-stats-c-abi-fixture-v1';passed=$false;before=(Get-ApiHashes);after=$null;
    source_hashes_match=$false;core_fixture_dll_built=$false;four_symbols_resolved_in_c=$false;
    bds_mod_loaded=$false;bds_started=$false;legacy_remote_call_compatible=$false;full_parity_accepted=$false;
    fixture_dll_sha256=$null;consumer_sha256=$null;error=$null}
Write-Output $apiBuild
Push-Location $apiBuild
try {
    $apiFlags=@('/nologo','/MD','/W4','/WX','/utf-8',"/I$apiProject/src")
    $apiDll=Join-Path $apiBuild 'bi_stats_fixture.dll'
    $apiSources=@('core/remote_api.cpp','core/stats_reader.cpp','core/sqlite_api.cpp','core/json.cpp','core/type_labels.cpp') |
        ForEach-Object {Join-Path $apiProject "src/bedrock_insight/$_"}
    $apiArguments=$apiFlags+@('/EHsc','/std:c++20','/LD',"/I$env:BEDROCKINSIGHT_SQLITE_ROOT/include")+
        $apiSources+@((Join-Path $apiProject 'tests/remote_api_fixture.cpp'),
        "$env:BEDROCKINSIGHT_SQLITE_ROOT/lib/sqlite3.lib","/Fe:$apiDll")
    Invoke-BdsLoggedNative $apiCompiler $apiArguments (Join-Path $apiBuild 'fixture-dll-compile.log')
    $apiReceipt.core_fixture_dll_built=$true
    $apiExe=Join-Path $apiBuild 'c_consumer.exe'
    Invoke-BdsLoggedNative $apiCompiler ($apiFlags+@('/std:c11','/TC',
        (Join-Path $apiProject 'tests/remote_api_dynamic_tests.c'),"/Fe:$apiExe")) (Join-Path $apiBuild 'c-consumer-compile.log')
    Invoke-BdsLoggedNative $apiExe @($apiDll,(Join-Path $apiBuild 'fixture.sqlite3')) (Join-Path $apiBuild 'c-consumer.log')
    $apiReceipt.four_symbols_resolved_in_c=$true
    # The C consumer compares complete fixture bytes before/after API reads.
    # Independently parse its actual JSON output and the closed SQLite file.
    $apiReadback='import json,sqlite3,sys; from pathlib import Path; rows=[json.loads(s[5:]) for s in Path(sys.argv[1]).read_text(encoding="utf-8-sig").splitlines() if s.startswith("JSON ")]; expected=(len(rows)==3 and rows[0]["total"]==3 and rows[1]["entries"][0]["name"]=="a\"b\\c\n" and rows[2]["total_entries"]==3 and rows[2]["entries"]==[] and not rows[2]["has_more"]); db=sqlite3.connect(Path(sys.argv[2]).resolve().as_uri()+"?mode=ro",uri=True); integrity=db.execute("PRAGMA integrity_check").fetchone()[0]; db.close(); sys.exit("JSON/read-only integrity mismatch" if not expected or integrity!="ok" else 0)'
    Invoke-BdsLoggedNative 'python' @('-W','error::ResourceWarning','-c',$apiReadback,
        (Join-Path $apiBuild 'c-consumer.log'),(Join-Path $apiBuild 'fixture.sqlite3')) (Join-Path $apiBuild 'readback.log')
    $apiReceipt.fixture_dll_sha256=(Get-FileHash -LiteralPath $apiDll).Hash
    $apiReceipt.consumer_sha256=(Get-FileHash -LiteralPath $apiExe).Hash
    $apiReceipt.after=Get-ApiHashes
    $apiReceipt.source_hashes_match=(ConvertTo-Json $apiReceipt.before -Compress) -ceq (ConvertTo-Json $apiReceipt.after -Compress)
    if(-not $apiReceipt.source_hashes_match){throw 'API source drift during tests'}
    $apiReceipt.passed=$true
    Get-Content (Join-Path $apiBuild 'c-consumer.log')
}catch{$apiReceipt.error=$_.Exception.Message;throw}
finally {
    $apiReceipt|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $apiBuild 'api-evidence.json') -Encoding utf8
    Pop-Location
}
