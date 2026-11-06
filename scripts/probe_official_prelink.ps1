# Execute only the hash-verified build tool, on owned copies. Never load BDS.
param([Parameter(Mandatory)][string]$CandidateRoot,
      [ValidateSet('BuildRule','ReportedUsage')][string]$Syntax='BuildRule')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidate=[IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\')
if (-not $candidate.StartsWith($project + '\.build\server-candidate\',[StringComparison]::OrdinalIgnoreCase) -or
    [IO.Path]::GetFileName($candidate) -cnotmatch '^run-[a-f0-9]{32}$' -or
    [IO.Path]::GetDirectoryName($candidate) -ine ($project + '\.build\server-candidate')) {
    throw 'Prelink probe requires a project-local server candidate run'
}
Assert-BdsUnredirectedTree $candidate
$candidateEvidencePath=Join-Path $candidate 'candidate-evidence.json'
$candidateEvidence=Get-Content -Raw -LiteralPath $candidateEvidencePath | ConvertFrom-Json
$profilePath=Join-Path $project 'packaging/bds-profile.json'
if ($candidateEvidence.artifact_kind -cne 'official-server-function-import-candidate' -or
    $candidateEvidence.official_server_functions_verified -ne $true -or
    $candidateEvidence.bds_profile_sha256 -ine (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash -or
    (Get-FileHash -LiteralPath (Join-Path $candidate 'BedrockInsight.dll') -Algorithm SHA256).Hash -ine $candidateEvidence.dll_sha256) {
    throw 'Candidate identity/profile mismatch'
}
$runtime=& (Join-Path $PSScriptRoot 'inspect_official_runtime.ps1') -PassThru
$stage=Get-Content -Raw -LiteralPath $runtime.staging_evidence | ConvertFrom-Json
$tool=@($stage.archives | Where-Object { $_.id -ceq 'prelink' })[0].selected_files[0]
$data=@($stage.archives | Where-Object { $_.id -ceq 'bedrockdata' })[0].selected_files[0]
$root=Join-Path $project ('.build/prelink-probe/run-' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $root
foreach ($dir in @('inputs','output/lib','tmp')) { New-Item -ItemType Directory -Path (Join-Path $root $dir) -Force | Out-Null }
$objects=@('mod','runtime','admin_api','event','rollback','sqlite_api','storage') | ForEach-Object { Join-Path $candidate ($_.ToString()+'.obj') }
$llLibrary=Join-Path $candidate 'LeviLamina.functions.lib'
if ((Get-FileHash -LiteralPath $llLibrary -Algorithm SHA256).Hash -ine $candidateEvidence.import_library_sha256) { throw 'Candidate import library mismatch' }
if (-not $env:BEDROCKINSIGHT_SQLITE_ROOT) { throw 'BEDROCKINSIGHT_SQLITE_ROOT is required' }
$sqliteLibrary=Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'
if ((Get-FileHash -LiteralPath $sqliteLibrary -Algorithm SHA256).Hash -ine $candidateEvidence.sqlite_library_sha256) { throw 'SQLite library mismatch' }
$records=@(); $copies=@()
foreach ($source in @($objects)+@($llLibrary,$sqliteLibrary)) {
    Assert-BdsUnredirectedAncestors $source
    $hash=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash
    $copy=Join-Path $root ('inputs/' + [IO.Path]::GetFileName($source))
    Copy-Item -LiteralPath $source -Destination $copy
    if ((Get-FileHash -LiteralPath $copy -Algorithm SHA256).Hash -ine $hash) { throw 'Prelink input copy mismatch' }
    $records += [pscustomobject]@{source=$source;source_sha256=$hash;copy=$copy;copy_sha256_before=$hash}
    $copies+=$copy
}
$arguments=if ($Syntax -ceq 'BuildRule') {
    @('server-windows-x64',(Join-Path $root 'output'),$data.path)+$copies
} else { @($data.path,(Join-Path $root 'output'))+$copies }
$before=[ordered]@{created_utc=[DateTime]::UtcNow.ToString('o');runtime_inspection=$runtime.evidence_path;
    candidate_evidence=$candidateEvidencePath;candidate_evidence_sha256=(Get-FileHash -LiteralPath $candidateEvidencePath -Algorithm SHA256).Hash;
    tool=$tool;data=$data;syntax=$Syntax;arguments=$arguments;inputs=$records;
    launch_authorized=$false;bds_load_verified=$false;probe_only=$true}
$before | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'prelink-input-evidence.json') -Encoding UTF8
# The cached official build rule and the tool's reported usage disagree.
# Keep both forms explicit for diagnosis; no silent retries or input mutation.
if ((Get-FileHash -LiteralPath $tool.path -Algorithm SHA256).Hash -ine $tool.sha256 -or
    (Get-FileHash -LiteralPath $data.path -Algorithm SHA256).Hash -ine $data.sha256) { throw 'Verified prelink/tool data changed' }
$previousTemp=$env:TEMP; $previousTmp=$env:TMP
$nativeFailure=$null
Push-Location -LiteralPath $root
try {
    $env:TEMP=Join-Path $root 'tmp'; $env:TMP=$env:TEMP
    Invoke-BdsLoggedNative $tool.path $arguments (Join-Path $root 'prelink.log')
} catch { $nativeFailure=$_
} finally { $env:TEMP=$previousTemp; $env:TMP=$previousTmp; Pop-Location }
$postcheckErrors=@()
foreach ($record in $records) {
    try {
        if ((Get-FileHash -LiteralPath $record.source -Algorithm SHA256).Hash -ine $record.source_sha256) { throw 'Original prelink input changed' }
        $record | Add-Member -NotePropertyName copy_sha256_after -NotePropertyValue (Get-FileHash -LiteralPath $record.copy -Algorithm SHA256).Hash
    } catch { $postcheckErrors+=$_.Exception.Message }
}
Assert-BdsUnredirectedTree $root
$outputs=@(Get-ChildItem -LiteralPath (Join-Path $root 'output') -Recurse -File | ForEach-Object {
    [pscustomobject]@{path=$_.FullName;size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
})
$artifactFailure=$null
try {
    $api=@($outputs | Where-Object { [IO.Path]::GetFileName($_.path) -ceq 'bedrock_runtime_api.lib' })
    if ($api.Count -ne 1) { throw 'Missing/ambiguous prelink runtime API library' }
    Assert-BdsPrelinkOutput ([IO.Path]::GetFileName($api[0].path)) $api[0].size
} catch { $artifactFailure=$_.Exception.Message }
$success=($null -eq $nativeFailure -and $postcheckErrors.Count -eq 0 -and $null -eq $artifactFailure)
[pscustomobject]@{finished_utc=[DateTime]::UtcNow.ToString('o');input_evidence=(Join-Path $root 'prelink-input-evidence.json');
    tool_exit_zero=($null -eq $nativeFailure);generation_verified=$success;
    native_error=$(if ($nativeFailure) { $nativeFailure.Exception.Message } else { $null });
    original_input_postcheck_errors=$postcheckErrors;artifact_error=$artifactFailure;
    inputs=$records;outputs=$outputs;normal_release_evidence=$false;
    matched_server_sdk_verified=$false;runtime_dependencies_verified=$false;launch_authorized=$false;bds_load_verified=$false;
    limits='Build-tool probe on copied real candidate objects/libraries. Original object hashes recorded now, not historical compile certification. Output not linked, DLL not loaded.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'prelink-output-evidence.json') -Encoding UTF8
if ($nativeFailure) { throw $nativeFailure }
if (-not $success) { throw "Prelink probe verification failed: $($postcheckErrors -join '; ') $artifactFailure" }
"Official prelink probe PASS; no game/DLL loaded. Evidence: $(Join-Path $root 'prelink-output-evidence.json')"
