# Prepare genuine BDS + official loader + capture candidate. Does NOT launch.
param([Parameter(Mandatory)][string]$CandidateRoot, [switch]$PassThru,
      [switch]$RealPlayer, [string]$PlayerName, [string]$PlayerXuid,
      [ValidateRange(49152,65535)][int]$ServerPort=59472,
      [ValidateRange(49152,65535)][int]$ServerPortV6=59473)
$ErrorActionPreference='Stop'
if ($ServerPort -eq $ServerPortV6) { throw 'Isolation ports must be distinct' }
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
. (Join-Path $PSScriptRoot 'validate_capture_isolation.ps1')
$access=New-BdsCaptureAccessPolicy $RealPlayer.IsPresent $PlayerName $PlayerXuid
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$candidate=[IO.Path]::GetFullPath($CandidateRoot).TrimEnd('\')
if ([IO.Path]::GetDirectoryName($candidate) -ine ($project + '\.build\server-candidate') -or
    [IO.Path]::GetFileName($candidate) -cnotmatch '^run-[a-f0-9]{32}$') { throw 'Expected project-local server candidate' }
Assert-BdsUnredirectedTree $candidate
$buildEvidencePath=Join-Path $candidate 'candidate-evidence.json'
$buildSnapshot=Read-BdsJsonSnapshot $buildEvidencePath; $build=$buildSnapshot.data
$profilePath=Join-Path $project 'packaging/bds-profile.json'
$profileSnapshot=Read-BdsJsonSnapshot $profilePath; $profile=$profileSnapshot.data
if ($build.artifact_kind -cne 'official-server-function-import-candidate' -or
    $build.prelink_completed -ne $true -or $build.official_server_functions_verified -ne $true -or
    $build.old_client_import_library_used -ne $false -or
    $build.bds_profile_sha256 -ine $profileSnapshot.sha256 -or
    (Get-FileHash -LiteralPath (Join-Path $candidate 'BedrockInsight.dll') -Algorithm SHA256).Hash -ine $build.dll_sha256) {
    throw 'Candidate build/profile mismatch'
}
Assert-BdsCaptureBuild $build $profile $profileSnapshot.sha256 $build.dll_sha256
$runtime=& (Join-Path $PSScriptRoot 'inspect_official_runtime.ps1') -PassThru
$stage=Get-Content -Raw -LiteralPath $runtime.staging_evidence | ConvertFrom-Json
$pinsSnapshot=Read-BdsJsonSnapshot (Join-Path $project 'packaging/official-inputs.json'); $pins=$pinsSnapshot.data
$root=Join-Path $project ('.build/isolated-bds/run-' + [guid]::NewGuid().ToString('N'))
$root=Assert-BdsSandboxPath $project $root
Assert-BdsUnredirectedAncestors $root
New-Item -ItemType Directory -Path $root | Out-Null
$archive=Join-Path $project ('.build/bds-downloads/' + $pins.bds.name)
Assert-BdsUnredirectedAncestors $archive
$stream=[IO.File]::Open($archive,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
$files=@()
try {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
    Assert-BdsInputIdentity $pins.bds $stream.Length $hash
    $stream.Position=0
    $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read,$true)
    try {
        if ($zip.Entries.Count -gt 10000) { throw 'Excessive BDS ZIP entry count' }
        $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase); $total=0L
        foreach ($entry in $zip.Entries) {
            Assert-BdsInputEntry $entry.FullName $entry.Length $entry.ExternalAttributes 268435456
            if (-not $seen.Add($entry.FullName)) { throw 'Duplicate BDS ZIP entry' }
            $total+=$entry.Length
            if ($total -gt 536870912) { throw 'Excessive BDS ZIP expanded size' }
            if ($entry.FullName -match '^(?i:worlds|mods|plugins)(/|$)') { throw 'Unexpected preexisting world/mod in BDS ZIP' }
        }
        foreach ($entry in $zip.Entries) {
            $target=Join-Path $root $entry.FullName
            Assert-BdsUnredirectedAncestors $target
            if ($entry.FullName.EndsWith('/')) { New-Item -ItemType Directory -Path $target -Force | Out-Null; continue }
            New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
            $payload=$entry.Open()
            try {
                $output=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                try { $payload.CopyTo($output); if ($output.Length -ne $entry.Length) { throw 'BDS payload length mismatch' } }
                finally { $output.Dispose() }
            } finally { $payload.Dispose() }
            $files+=[pscustomobject]@{path=$entry.FullName;size=$entry.Length;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash}
        }
    } finally { $zip.Dispose() }
} finally { $stream.Dispose() }
$placements=@(
    @{id='preloader';entry='bin/PreLoader.dll';dest='PreLoader.dll'},
    @{id='bedrockdata';entry='bedrock_runtime_data';dest='bedrock_runtime_data'},
    @{id='peeditor';entry='PeEditor.exe';dest='PeEditor.exe'},
    @{id='crashlogger';entry='CrashLogger.exe';dest='mods/LeviLamina/CrashLogger.exe'},
    @{id='levilamina';entry='LeviLamina/LeviLamina.dll';dest='mods/LeviLamina/LeviLamina.dll'},
    @{id='levilamina';entry='LeviLamina/manifest.json';dest='mods/LeviLamina/manifest.json'}
)
foreach ($placement in $placements) {
    $inputArchive=@($stage.archives | Where-Object { $_.id -ceq $placement.id })[0]
    $file=@($inputArchive.selected_files | Where-Object { $_.entry -ceq $placement.entry })[0]
    $dest=Join-Path $root $placement.dest
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null
    Copy-Item -LiteralPath $file.path -Destination $dest
    if ((Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash -ine $file.sha256) { throw 'Runtime placement hash mismatch' }
    $files+=[pscustomobject]@{path=$placement.dest;size=$file.size;sha256=$file.sha256}
}
foreach ($name in @('BedrockInsight.dll','manifest.json')) {
    $source=Join-Path $candidate $(if ($name -ceq 'manifest.json') { 'candidate-package/BedrockInsight/manifest.json' } else { $name })
    $dest=Join-Path $root ('mods/BedrockInsight/' + $name)
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($dest)) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $dest
    $files+=[pscustomobject]@{path=('mods/BedrockInsight/' + $name);size=([IO.FileInfo]::new($dest)).Length;sha256=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash}
}
# Configuration-only fresh world; no existing files/worlds/ports are reused.
$runId=[IO.Path]::GetFileName($root).Substring(4)
@"
server-name=BedrockInsight isolated capture test
transport=nethernet
server-port=$ServerPort
server-udp-ports=$ServerPort
server-portv6=$ServerPortV6
enable-lan-visibility=false
allow-list=true
online-mode=true
max-players=1
default-player-permission-level=visitor
allow-cheats=false
level-name=insight-isolated-$runId
view-distance=4
tick-distance=4
"@ | Set-Content -LiteralPath (Join-Path $root 'server.properties') -Encoding ASCII
Get-BdsCaptureAllowlistJson $access | Set-Content -LiteralPath (Join-Path $root 'allowlist.json') -Encoding UTF8
'[]' | Set-Content -LiteralPath (Join-Path $root 'permissions.json') -Encoding ASCII
$configDir=Join-Path $root 'mods/LeviLamina/config'
New-Item -ItemType Directory -Path $configDir | Out-Null
# Deliberately trigger the pinned v42 official defaultConfigUpdater. A partial
# document labelled CURRENT v42 fails required-field deserialization and can
# fall back to defaults, silently losing these safety overrides. v41 is merged
# into a complete default document by the official updater before deserializing.
@{version=41;language='en';modules=@{crashLogger=@{enabled=$false;builtin=$false;uploadToSentry=$false};
    command=@{crashCommand=@{enabled=$false}}};
    targeted=@{enableStatistics=$false;checkRunningBDS=$false;forceEnableCheatCommands=$false;preserveInputLine=$false;
        netherNetPatch=@{enable=$false};playerInfo=@{alwaysLaunch=$false}}
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $configDir 'Config.json') -Encoding UTF8
# Replace hash records for the three deliberately hardened factory config files.
foreach ($name in @('server.properties','allowlist.json','permissions.json','mods/LeviLamina/config/Config.json')) {
    $files=@($files | Where-Object { $_.path -cne $name })
    $file=Join-Path $root $name
    $files+=[pscustomobject]@{path=$name;size=([IO.FileInfo]::new($file)).Length;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
}
Assert-BdsUnredirectedTree $root
foreach ($snapshot in @($buildSnapshot,$profileSnapshot,$pinsSnapshot)) { Assert-BdsJsonSnapshotUnchanged $snapshot }
[pscustomobject]@{prepared_utc=[DateTime]::UtcNow.ToString('o');sandbox_root=$root;
    capture_access=$access;
    bds_archive=$archive;bds_archive_sha256=$hash;bds_version=$pins.bds.version;
    bds_profile_sha256=$profileSnapshot.sha256;pins_sha256=$pinsSnapshot.sha256;
    candidate_root=$candidate;candidate_evidence_sha256=$buildSnapshot.sha256;
    runtime_inspection=$runtime.evidence_path;runtime_staging_evidence=$runtime.staging_evidence;files=$files;
    matched_server_sdk_verified=$false;normal_release_evidence=$false;
    launch_authorized=$false;bds_load_verified=$false;
    limits='Fresh genuine BDS input and capture candidate staged only. No PeEditor or server execution, no localization yet, no isolation authorization.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $root 'capture-staging-evidence.json') -Encoding UTF8
if ($PassThru) { [pscustomobject]@{sandbox_root=$root;evidence_path=(Join-Path $root 'capture-staging-evidence.json')} }
else { "Isolated capture staging PASS; NO execution. Root: $root" }
