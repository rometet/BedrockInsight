# Genuine server-derived import metadata + pinned headers; never launches BDS.
param([string]$ReferenceSdkRoot='C:\Dev\LeviLamina-26.60-Assistant-20261029')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
. (Join-Path $PSScriptRoot 'validate_import_metadata.ps1')
. (Join-Path $PSScriptRoot 'server_function_exports.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
. (Join-Path $PSScriptRoot 'validate_symbol_provider.ps1')
foreach($name in @('BEDROCKINSIGHT_CLANGCL_ROOT','BEDROCKINSIGHT_SQLITE_ROOT','BEDROCKINSIGHT_XMAKE_PACKAGES')) {
    if(-not [Environment]::GetEnvironmentVariable($name)){throw "$name is required"}
}
$reader=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
$dlltool=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-dlltool.exe'
$sqliteLibrary=Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'
foreach($path in @($reader,$dlltool,$sqliteLibrary)){if(-not [IO.File]::Exists($path)){throw "Missing tool/library: $path"}}
$profilePath=Join-Path $projectRoot 'packaging/bds-profile.json'
$profileHash=(Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
$profile=Get-Content -Raw -LiteralPath $profilePath|ConvertFrom-Json
$sourceFiles=@('levilamina/mod.cpp','levilamina/runtime.cpp','levilamina/busy_tick.cpp','levilamina/player_actions.cpp','levilamina/player_death.cpp','levilamina/player_modes.cpp','levilamina/player_pickup.cpp','levilamina/player_jump.cpp','levilamina/player_kills.cpp','levilamina/player_hurt.cpp','levilamina/player_item_use.cpp','levilamina/player_item_transfer.cpp','levilamina/player_block.cpp','levilamina/player_block_interaction.cpp','levilamina/player_travel.cpp','levilamina/player_damage.cpp','levilamina/player_trade.cpp','levilamina/player_breed.cpp','levilamina/player_target.cpp','levilamina/stats_command.cpp','core/admin_api.cpp','core/event.cpp','core/rollback.cpp','core/sqlite_api.cpp','core/storage.cpp','core/stats_reader.cpp','core/remote_api.cpp')|
    ForEach-Object{Join-Path $projectRoot "src/bedrock_insight/$_"}
$sourceFiles+=@(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'src/bedrock_insight') -File -Recurse|
    Where-Object{$_.Extension -in @('.h','.hpp')}|ForEach-Object{$_.FullName})
$sourceHashes=@($sourceFiles|Sort-Object -Unique|ForEach-Object{[pscustomobject]@{path=$_;sha256=(Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash}})
$buildScriptHashes=@('build_server_candidate.ps1','verify_pinned_sdk_headers.ps1','validate_server_release.ps1' |
    ForEach-Object {$path=Join-Path $PSScriptRoot $_;[pscustomobject]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}})
$sqliteHash=(Get-FileHash -LiteralPath $sqliteLibrary -Algorithm SHA256).Hash
$symbolProvider=Get-BdsSymbolProvider $env:BEDROCKINSIGHT_XMAKE_PACKAGES
$runtimeInputs=& (Join-Path $PSScriptRoot 'inspect_official_runtime.ps1') -PassThru
$runtimeStage=Get-Content -Raw -LiteralPath $runtimeInputs.staging_evidence | ConvertFrom-Json
$prelinkTool=@($runtimeStage.archives | Where-Object { $_.id -ceq 'prelink' })[0].selected_files[0]
$runtimeData=@($runtimeStage.archives | Where-Object { $_.id -ceq 'bedrockdata' })[0].selected_files[0]
$inspection=& (Join-Path $PSScriptRoot 'inspect_server_release.ps1') -PassThru
$serverEvidence=Get-Content -Raw -LiteralPath $inspection.evidence_path|ConvertFrom-Json
if(-not $serverEvidence.official_server_archive_verified -or $serverEvidence.bds_profile_sha256 -ine $profileHash){throw 'Unverified server archive evidence'}
$serverDll=$serverEvidence.server_dll
$serverPe=@(& $reader --file-headers --sections --coff-exports --coff-imports $serverDll)
if($LASTEXITCODE -ne 0){throw 'Server function inspection failed'}
$classified=Get-BdsServerFunctionExports $serverPe
$buildRoot=Join-Path $projectRoot ('.build/server-candidate/run-' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $buildRoot
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'tmp') -Force|Out-Null
# Keep the PRE-build snapshot even if a later validation gate rejects the DLL.
# It is evidence, not authorization to skip any compile/link/final hash gate.
$sourceHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $buildRoot 'source-hashes-before.json') -Encoding UTF8
$buildScriptHashes|ConvertTo-Json -Depth 4|Set-Content -LiteralPath (Join-Path $buildRoot 'build-script-hashes-before.json') -Encoding UTF8
'OFFICIAL SERVER-LINKED CANDIDATE ONLY. NO BDS LOAD/ABI/RUNTIME ACCEPTANCE. DO NOT DEPLOY TO LIVE SERVERS.'|
    Set-Content -LiteralPath (Join-Path $buildRoot 'NOT_RUNTIME_VERIFIED.txt') -Encoding UTF8
$definition=Join-Path $buildRoot 'LeviLamina.functions.def'
$importLibrary=Join-Path $buildRoot 'LeviLamina.functions.lib'
@('LIBRARY LeviLamina.dll','EXPORTS') + @($classified.functions|ForEach-Object{'    "' + $_.name + '"'})|
    Set-Content -LiteralPath $definition -Encoding ASCII
& $dlltool -m i386:x86-64 -D LeviLamina.dll -d $definition -l $importLibrary *> (Join-Path $buildRoot 'dlltool.log')
if($LASTEXITCODE -ne 0){throw 'Server function import library generation failed'}
$metadata=@(& $reader --file-headers --sections $importLibrary)
if($LASTEXITCODE -ne 0){throw 'Generated import library inspection failed'}
$shape=Assert-NamedLlImportMetadata $metadata
$generatedNames=@($metadata|ForEach-Object{if($_ -match '^Export name: (.+)$'){$Matches[1]}})
$expectedNames=@($classified.functions|ForEach-Object{$_.name})
if($generatedNames.Count -ne $expectedNames.Count -or @($generatedNames|Where-Object{$_ -cnotin $expectedNames}).Count -or
    @($generatedNames|Select-Object -Unique).Count -ne $expectedNames.Count){throw 'Generated import library names differ from verified function exports'}
$metadata|Set-Content -LiteralPath (Join-Path $buildRoot 'import-library-pe.txt') -Encoding UTF8
$importHash=(Get-FileHash -LiteralPath $importLibrary -Algorithm SHA256).Hash
# This old source root supplies recorded THIRD-PARTY header dependencies only.
# No old MC/LL headers or old client import library enter the candidate link.
$headerRoot=Join-Path $buildRoot 'native-header-compile'
Assert-BdsUnredirectedAncestors $headerRoot
if([IO.Directory]::Exists($headerRoot)){Assert-BdsUnredirectedTree $headerRoot}
& (Join-Path $PSScriptRoot 'verify_pinned_sdk_headers.ps1') -ReferenceSdkRoot $ReferenceSdkRoot -BuildOutputRoot $headerRoot
$candidateHeaderEvidence=Join-Path $buildRoot 'header-compile-evidence.json'
Copy-Item -LiteralPath (Join-Path $headerRoot 'compile-evidence.json') -Destination $candidateHeaderEvidence
$compile=Get-Content -Raw -LiteralPath $candidateHeaderEvidence|ConvertFrom-Json
if($compile.header_source_commit -cne $profile.levilamina_commit -or $compile.header_catalog_sha256 -ine $profile.header_catalog_sha256){throw 'Pinned header/profile mismatch'}
$env:TEMP=Join-Path $buildRoot 'tmp';$env:TMP=$env:TEMP
$nativeObjects=@();$nativeObjectHashes=@()
foreach($object in $compile.objects){
    if((Get-FileHash -LiteralPath $object.path -Algorithm SHA256).Hash -ine $object.sha256){throw 'Native object changed'}
    $copy=Join-Path $buildRoot ([IO.Path]::GetFileName($object.path))
    Copy-Item -LiteralPath $object.path -Destination $copy
    if((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ine $object.sha256){throw 'Native object copy mismatch'}
    $nativeObjects+=$copy
    $nativeObjectHashes+=[pscustomobject]@{path=$copy;sha256=$object.sha256}
}
$coreNames=@('admin_api','event','rollback','sqlite_api','storage','stats_reader','remote_api')
$coreArguments=@($compile.compile_options)
foreach($name in $coreNames){$coreArguments+=Join-Path $projectRoot "src/bedrock_insight/core/$name.cpp"}
$dll=Join-Path $buildRoot 'BedrockInsight.dll'
$linkArguments=@('/nologo','/LD','/MD',"/Fe:$dll") + $nativeObjects
foreach($name in $coreNames){$linkArguments+=Join-Path $buildRoot "$name.obj"}
$linkArguments+=@('/link','/DEBUG',"/PDB:$(Join-Path $buildRoot 'BedrockInsight.pdb')",
    "/IMPLIB:$(Join-Path $buildRoot 'BedrockInsight.lib')",$importLibrary,$sqliteLibrary,$symbolProvider.path)
Push-Location -LiteralPath $buildRoot
try{
    Invoke-BdsLoggedNative $compile.compiler $coreArguments (Join-Path $buildRoot 'core-compile.log')
    # prelink may rewrite its inputs: give it OWNED COPIES, including static
    # libraries. Nothing in C:\Dev, caches or earlier runs can be modified.
    $prelinkRoot=Join-Path $buildRoot 'prelink'
    foreach($dir in @('inputs','output/lib')){New-Item -ItemType Directory -Path (Join-Path $prelinkRoot $dir) -Force|Out-Null}
    $prelinkSources=@($nativeObjects)
    foreach($name in $coreNames){$prelinkSources+=Join-Path $buildRoot "$name.obj"}
    $prelinkSources+=@($importLibrary,$sqliteLibrary,$symbolProvider.path)
    $prelinkRecords=@();$prelinkCopies=@()
    foreach($source in $prelinkSources){
        $hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
        $copy=Join-Path $prelinkRoot ('inputs/' + [IO.Path]::GetFileName($source))
        Copy-Item -LiteralPath $source -Destination $copy
        if((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ine $hash){throw 'Prelink input copy mismatch'}
        $prelinkCopies+=$copy
        $prelinkRecords+=[pscustomobject]@{source=$source;source_sha256=$hash;copy=$copy;copy_sha256_before=$hash}
    }
    $prelinkArguments=@('server-windows-x64',(Join-Path $prelinkRoot 'output'),$runtimeData.path)+$prelinkCopies
    if((Get-FileHash -LiteralPath $prelinkTool.path -Algorithm SHA256).Hash -ine $prelinkTool.sha256 -or
        (Get-FileHash -LiteralPath $runtimeData.path -Algorithm SHA256).Hash -ine $runtimeData.sha256){throw 'Verified prelink tool/data changed'}
    Invoke-BdsLoggedNative $prelinkTool.path $prelinkArguments (Join-Path $prelinkRoot 'prelink.log')
    foreach($record in $prelinkRecords){
        if((Get-FileHash -LiteralPath $record.source -Algorithm SHA256).Hash -ine $record.source_sha256){throw 'Prelink modified an original input'}
        $record|Add-Member -NotePropertyName copy_sha256_after -NotePropertyValue (Get-FileHash -LiteralPath $record.copy -Algorithm SHA256).Hash
    }
    Assert-BdsUnredirectedTree $prelinkRoot
    $runtimeApi=Join-Path $prelinkRoot 'output/lib/bedrock_runtime_api.lib'
    if(-not [IO.File]::Exists($runtimeApi)){throw 'Missing actual prelink runtime API library'}
    Assert-BdsPrelinkOutput ([IO.Path]::GetFileName($runtimeApi)) ([IO.FileInfo]::new($runtimeApi).Length)
    $runtimeApiHash=(Get-FileHash -LiteralPath $runtimeApi -Algorithm SHA256).Hash
    $apiInspection=@(& $reader --file-headers --sections $runtimeApi)
    if($LASTEXITCODE -ne 0){throw 'Generated runtime API inspection failed'}
    $apiInspection|Set-Content -LiteralPath (Join-Path $prelinkRoot 'runtime-api-pe.txt') -Encoding UTF8
    # Link the processed copies, not the original pre-prelink objects/libraries.
    for($index=0;$index -lt $linkArguments.Count;$index++){
        $record=@($prelinkRecords|Where-Object{$_.source -ceq $linkArguments[$index]})
        if($record.Count -eq 1){$linkArguments[$index]=$record[0].copy}
    }
    # The regular MSVC helper attempts to load a nonexistent runtime DLL and
    # raises C06D007E on first MCAPI call. LL's pinned SymbolProvider resolves
    # this virtual module through ll::memory::resolveSymbol instead.
    $linkArguments+=@($runtimeApi,'/DELAYLOAD:bedrock_runtime.dll','/NODEFAULTLIB:delayimp.lib')
    Invoke-BdsLoggedNative $compile.compiler $linkArguments (Join-Path $buildRoot 'link.log')
}finally{Pop-Location}
$modPe=@(& $reader --file-headers --coff-exports --coff-imports $dll)
if($LASTEXITCODE -ne 0){throw 'Candidate PE inspection failed'}
$resolution=Test-BdsServerImportResolution $serverPe $modPe -AllowIntervalRuntimeApi
foreach($name in $resolution.mod_ll_import_names){if($name -cnotin $expectedNames){throw 'Candidate imported an excluded/nonfunction export'}}
if((Get-FileHash -LiteralPath $serverDll -Algorithm SHA256).Hash -ine $serverEvidence.server_dll_sha256 -or
    (Get-FileHash -LiteralPath $importLibrary -Algorithm SHA256).Hash -ine $importHash -or
    (Get-FileHash -LiteralPath $sqliteLibrary -Algorithm SHA256).Hash -ine $sqliteHash -or
    (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash -ine $profileHash){throw 'Verified build input changed'}
foreach($record in $sourceHashes){if((Get-FileHash -LiteralPath $record.path -Algorithm SHA256).Hash -ine $record.sha256){throw 'C++ source/header changed during build'}}
foreach($record in $buildScriptHashes){if((Get-FileHash -LiteralPath $record.path -Algorithm SHA256).Hash -ine $record.sha256){throw 'Build/PE-policy script changed during build'}}
$modPe|Set-Content -LiteralPath (Join-Path $buildRoot 'mod-pe.txt') -Encoding UTF8
$serverPe|Set-Content -LiteralPath (Join-Path $buildRoot 'server-pe.txt') -Encoding UTF8
$nativeManifestPath=Join-Path $projectRoot 'manifest.json'
$nativeManifest=Get-Content -Raw -LiteralPath $nativeManifestPath|ConvertFrom-Json
Assert-BdsNativeCaptureManifest $nativeManifest $profile
$packageRoot=Join-Path $buildRoot 'candidate-package'
$packageMod=Join-Path $packageRoot 'BedrockInsight'
New-Item -ItemType Directory -Path $packageMod|Out-Null
foreach($path in @($dll,(Join-Path $buildRoot 'BedrockInsight.pdb'),$nativeManifestPath)){Copy-Item -LiteralPath $path -Destination $packageMod}
Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $packageMod
New-Item -ItemType Directory -Path (Join-Path $packageMod 'licenses') -Force|Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot 'licenses/Pier-Apache-2.0.txt') -Destination (Join-Path $packageMod 'licenses')
Copy-Item -LiteralPath (Join-Path $buildRoot 'NOT_RUNTIME_VERIFIED.txt') -Destination $packageRoot
$zip=Join-Path $buildRoot 'BedrockInsight-server-candidate.zip'
# Manual-install mod folder only; no Tooth installer/runtime/SDK facade.
& tar.exe -a -c -f $zip -C $packageRoot BedrockInsight NOT_RUNTIME_VERIFIED.txt
if($LASTEXITCODE -ne 0){throw 'Candidate archive creation failed'}
[ordered]@{
    artifact_kind='official-server-function-import-candidate';built_utc=[DateTime]::UtcNow.ToString('o')
    official_server_functions_verified=$true;old_client_import_library_used=$false
    matched_server_sdk_verified=$false;runtime_dependencies_verified=$false;normal_release_evidence=$false
    candidate_packaged=$true;launch_authorized=$false;bds_load_verified=$false
    source_hashes=$sourceHashes;header_compile_evidence=$candidateHeaderEvidence;native_objects=$nativeObjectHashes
    build_script_hashes=$buildScriptHashes
    header_source_commit=$compile.header_source_commit;header_catalog_sha256=$compile.header_catalog_sha256
    header_dependencies=$compile.header_dependencies;server_archive_evidence=$inspection.evidence_path
    server_dll_sha256=$serverEvidence.server_dll_sha256;function_exports=$classified;resolution=$resolution
    import_library=$importLibrary;import_library_sha256=$importHash;import_library_shape=$shape
    sqlite_library=$sqliteLibrary;sqlite_library_sha256=$sqliteHash
    symbol_provider=$symbolProvider;msvc_delayimp_used=$false
    official_runtime_input_inspection=$runtimeInputs.evidence_path
    prelink_completed=$true;prelink_tool=$prelinkTool;runtime_data=$runtimeData
    prelink_arguments=$prelinkArguments;prelink_input_copies=$prelinkRecords
    runtime_api_library=$runtimeApi;runtime_api_library_sha256=$runtimeApiHash
    compiler=$compile.compiler;core_arguments=$coreArguments;link_arguments=$linkArguments
    bds_profile_sha256=$profileHash;dll=$dll;dll_sha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
    candidate_zip=$zip;candidate_zip_sha256=(Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
} |ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $buildRoot 'candidate-evidence.json') -Encoding UTF8
'SERVER-DERIVED NATIVE CANDIDATE BUILD PASS; no full SDK or BDS runtime acceptance'
Get-FileHash -LiteralPath $dll,$zip -Algorithm SHA256|Format-List
"Evidence: $(Join-Path $buildRoot 'candidate-evidence.json')"
