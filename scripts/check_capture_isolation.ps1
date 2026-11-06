# Read-only candidate-specific policy. Does not fabricate normal SDK evidence.
param([Parameter(Mandatory)][string]$SandboxRoot, [switch]$PassThru, [switch]$RequirePatched,
      [switch]$RealPlayer, [string]$PlayerName, [string]$PlayerXuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
$access=New-BdsCaptureAccessPolicy $RealPlayer.IsPresent $PlayerName $PlayerXuid
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Assert-BdsSandboxPath $project $SandboxRoot
Assert-BdsUnredirectedTree $root
$stage=Get-Content -Raw -LiteralPath (Join-Path $root 'capture-staging-evidence.json') | ConvertFrom-Json
# Old empty-smoke evidence remains inspectable; a real-player run always needs
# explicit policy metadata AND the same identity supplied at every stage.
if ($null -ne $stage.capture_access -or $RealPlayer) { Assert-BdsCaptureAccessPolicy $stage.capture_access $access }
$profilePath=Join-Path $project 'packaging/bds-profile.json'
$profile=Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
$profileHash=(Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
$pins=Get-Content -Raw -LiteralPath (Join-Path $project 'packaging/official-inputs.json') | ConvertFrom-Json
if ($stage.sandbox_root -ine $root -or $stage.bds_profile_sha256 -ine $profileHash -or
    $stage.bds_archive_sha256 -ine $pins.bds.sha256 -or $stage.bds_version -cne $profile.bds_version) {
    throw 'Isolation staging/profile mismatch'
}
$candidate=[IO.Path]::GetFullPath($stage.candidate_root).TrimEnd('\')
if ([IO.Path]::GetDirectoryName($candidate) -ine ($project + '\.build\server-candidate') -or
    [IO.Path]::GetFileName($candidate) -cnotmatch '^run-[a-f0-9]{32}$') { throw 'Unowned capture candidate' }
Assert-BdsUnredirectedTree $candidate
$buildPath=Join-Path $candidate 'candidate-evidence.json'
if ((Get-FileHash -LiteralPath $buildPath -Algorithm SHA256).Hash -ine $stage.candidate_evidence_sha256) { throw 'Candidate evidence changed' }
$build=Get-Content -Raw -LiteralPath $buildPath | ConvertFrom-Json
$dllHash=(Get-FileHash -LiteralPath (Join-Path $root 'mods/BedrockInsight/BedrockInsight.dll') -Algorithm SHA256).Hash
Assert-BdsCaptureBuild $build $profile $profileHash $dllHash
# Recheck every freshly extracted BDS/runtime/mod/config file. No unknown files
# (including old worlds, extra mods, plugin configs or links) can enter the run.
$paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$records=@($stage.files)
if ($RequirePatched) {
    $patch=Get-Content -Raw -LiteralPath (Join-Path $root 'capture-patch-evidence.json') | ConvertFrom-Json
    if ($patch.sandbox_root -ine $root -or $patch.preloader_injection_verified -isnot [bool] -or
        -not $patch.preloader_injection_verified -or @($patch.files).Count -ne 4 -or
        $patch.source_exe_sha256 -ine (Get-FileHash -LiteralPath (Join-Path $root 'bedrock_server.exe') -Algorithm SHA256).Hash -or
        $patch.peeditor_sha256 -ine (Get-FileHash -LiteralPath (Join-Path $root 'PeEditor.exe') -Algorithm SHA256).Hash) { throw 'Capture patch identity mismatch' }
    foreach ($file in $patch.files) {
        if ($file.path -cnotin @('bedrock_server_mod.exe','PeEditor-build.log','patched-server-pe.txt','mods/LeviLamina/lang/en.json')) { throw 'Unreviewed capture patch output' }
    }
    $records+=@($patch.files)
}
foreach ($file in $records) {
    Assert-BdsInputEntry $file.path $file.size 0 268435456
    if (-not $paths.Add($file.path)) { throw 'Duplicate staged capture file record' }
    $path=Join-Path $root $file.path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $file.sha256) { throw "Staged capture file changed: $($file.path)" }
}
foreach ($file in Get-ChildItem -LiteralPath $root -File -Recurse -Force) {
    $relative=$file.FullName.Substring($root.Length+1).Replace('\','/')
    if (-not $paths.Contains($relative) -and $relative -cne 'capture-staging-evidence.json' -and
        -not ($RequirePatched -and $relative -ceq 'capture-patch-evidence.json')) { throw "Unknown staged capture file: $relative" }
}
foreach ($required in @('bedrock_server.exe','PreLoader.dll','bedrock_runtime_data','PeEditor.exe',
    'mods/LeviLamina/LeviLamina.dll','mods/LeviLamina/manifest.json','mods/BedrockInsight/BedrockInsight.dll','mods/BedrockInsight/manifest.json')) {
    if (-not $paths.Contains($required)) { throw "Missing capture file: $required" }
}
Assert-BdsServerReleaseManifest (Get-Content -Raw -LiteralPath (Join-Path $root 'mods/LeviLamina/manifest.json') | ConvertFrom-Json) $profile
$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'mods/BedrockInsight/manifest.json') | ConvertFrom-Json
Assert-BdsNativeCaptureManifest $manifest $profile
$config=Get-Content -Raw -LiteralPath (Join-Path $root 'mods/LeviLamina/config/Config.json') | ConvertFrom-Json
Assert-BdsCaptureLoaderConfig $config
$network=[Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties()
$modEntries=@(Get-ChildItem -LiteralPath (Join-Path $root 'mods') -Force)
if (@($modEntries | Where-Object { -not $_.PSIsContainer }).Count) { throw 'Unexpected file in mods root' }
$facts=[pscustomobject]@{sandbox_root=$root;actual_dll_sha256=$dllHash;
    server_properties=(Get-Content -Raw -LiteralPath (Join-Path $root 'server.properties'));
    allowlist_text=(Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'allowlist.json'));
    allowlist_empty=((Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root 'allowlist.json')) -match '^\s*\[\s*\]\s*$');
    permissions_empty=((Get-Content -Raw -LiteralPath (Join-Path $root 'permissions.json')) -match '^\s*\[\s*\]\s*$');
    world_directory_exists=(Test-Path -LiteralPath (Join-Path $root 'worlds'));
    mod_directories=@($modEntries.Name);
    bound_ports=@($network.GetActiveUdpListeners().Port)+@($network.GetActiveTcpListeners().Port);
    running_bds_paths=@(Get-BdsRunningExecutablePaths)}
$realAccess=$null
if ($RealPlayer) { $realAccess=$access }
$boundary=Test-BdsIsolationBoundary $facts $project $realAccess
$result=[pscustomobject]@{sandbox_root=$root;candidate_policy_passed=$boundary.candidate_policy_passed;
    capture_access=$access;
    checked_ports=$boundary.checked_ports;staged_file_count=$paths.Count;candidate_dll_sha256=$dllHash;
    normal_release_evidence=$false;matched_server_sdk_verified=$false;launch_authorized=$false;bds_load_verified=$false;
    limits='Separate capture-candidate policy, not normal SDK release acceptance. File hashes/config/world/process/ports checked now; no runtime ABI or post-check race proof.'}
if ($PassThru) { $result } else { $result | ConvertTo-Json -Depth 5 }
