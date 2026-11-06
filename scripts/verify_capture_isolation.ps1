$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
$profilePath=Join-Path $PSScriptRoot '../packaging/bds-profile.json'
$profile=Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
$hash=(Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
# Metadata only. These are not DLLs, SDK facades, release records or servers.
$build=[pscustomobject]@{artifact_kind='official-server-function-import-candidate';official_server_functions_verified=$true;
    prelink_completed=$true;old_client_import_library_used=$false;normal_release_evidence=$false;
    matched_server_sdk_verified=$false;bds_load_verified=$false;bds_profile_sha256=$hash;
    msvc_delayimp_used=$false;symbol_provider=[pscustomobject]@{
        source_commit='6c93ec45c8455992ee726d92df60316c8e731c44';
        sha256='B569E4BB25EE66FB9439D1D4B42C2EDCEF41878222ED9684B4118D986301ABFB'};
    header_source_commit=$profile.levilamina_commit;header_catalog_sha256=$profile.header_catalog_sha256;
    dll_sha256=('a'*64);resolution=[pscustomobject]@{mod_ll_imports_resolved=$true;
        mod_ll_import_names=@('?resolveSymbol@memory@ll@@YAPEAXPEBD@Z')}}
Assert-BdsCaptureBuild $build $profile $hash ('a'*64)
$changes=@(
    {param($x) $x.official_server_functions_verified='true'},
    {param($x) $x.prelink_completed=$false},
    {param($x) $x.old_client_import_library_used=$true},
    {param($x) $x.normal_release_evidence=$true},
    {param($x) $x.matched_server_sdk_verified=$true},
    {param($x) $x.bds_load_verified=$true},
    {param($x) $x.bds_profile_sha256='0'*64},
    {param($x) $x.header_source_commit='0'*40},
    {param($x) $x.header_catalog_sha256='0'*64},
    {param($x) $x.dll_sha256='0'*64},
    {param($x) $x.resolution.mod_ll_imports_resolved=$false},
    {param($x) $x.msvc_delayimp_used=$true},
    {param($x) $x.msvc_delayimp_used='false'},
    {param($x) $x.symbol_provider=$null},
    {param($x) $x.symbol_provider.source_commit='0'*40},
    {param($x) $x.symbol_provider.sha256='0'*64},
    {param($x) $x.resolution.mod_ll_import_names=@()},
    {param($x) $x.artifact_kind='client-probe'}
)
$serialized=$build | ConvertTo-Json -Depth 5
foreach ($change in $changes) {
    $bad=$serialized | ConvertFrom-Json; & $change $bad
    $rejected=$false
    try { Assert-BdsCaptureBuild $bad $profile $hash ('a'*64) } catch { $rejected=$true }
    if (-not $rejected) { throw 'Expected capture candidate rejection' }
}
"Capture candidate policy PASS ($($changes.Count) text-only rejections; shared isolation boundary tested separately)"
$config=[pscustomobject]@{version=41;language='en';modules=[pscustomobject]@{crashLogger=[pscustomobject]@{enabled=$false;uploadToSentry=$false}};
    targeted=[pscustomobject]@{enableStatistics=$false;checkRunningBDS=$false;forceEnableCheatCommands=$false;preserveInputLine=$false;
        netherNetPatch=[pscustomobject]@{enable=$false}}}
Assert-BdsCaptureLoaderConfig $config
$config.version=42
$rejected=$false
try { Assert-BdsCaptureLoaderConfig $config } catch { $rejected=$true }
if (-not $rejected) { throw 'Partial v42 config could fall back to unsafe defaults' }
$config.version=41; $config.targeted.enableStatistics='false'
$rejected=$false
try { Assert-BdsCaptureLoaderConfig $config } catch { $rejected=$true }
if (-not $rejected) { throw 'Nonboolean loader flag accepted' }
$config.targeted.enableStatistics=$false; $config.version='41'
$rejected=$false
try { Assert-BdsCaptureLoaderConfig $config } catch { $rejected=$true }
if (-not $rejected) { throw 'String loader schema version accepted' }
'Capture loader config PASS (3 regression rejections, including partial current-version config)'
