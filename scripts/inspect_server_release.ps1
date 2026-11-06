param([string]$ModDllPath, [switch]$PassThru)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
$modDll = $null
if ($ModDllPath) {
    $modDll = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($ModDllPath)) { $ModDllPath }
        else { Join-Path $projectRoot $ModDllPath }))
    if (-not $modDll.StartsWith($projectRoot + '\',[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetExtension($modDll) -ine '.dll') { throw 'Mod DLL must be project-local' }
    Assert-BdsUnredirectedAncestors $modDll
}
$reader = Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
$profilePath = Join-Path $projectRoot 'packaging/bds-profile.json'
$profile = Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
$archive = Join-Path $projectRoot ".build/bds-downloads/levilamina-v$($profile.levilamina_version)-server-release-windows-x64.zip"
Assert-BdsUnredirectedAncestors $archive
$output = Join-Path $projectRoot ('.build/server-release-inspection/run-' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $output
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
# Hold the verified bytes read-only, denying write/delete sharing through ZIP reads.
$archiveStream = [IO.File]::Open($archive,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    $hasher = [Security.Cryptography.SHA256]::Create()
    try { $archiveHash = [BitConverter]::ToString($hasher.ComputeHash($archiveStream)).Replace('-','') }
    finally { $hasher.Dispose() }
    Assert-BdsServerArchiveIdentity $archiveStream.Length $archiveHash $profile
    $archiveStream.Position = 0
    $zip = [IO.Compression.ZipArchive]::new($archiveStream,[IO.Compression.ZipArchiveMode]::Read,$true)
    try {
        $entryNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($entry in $zip.Entries) {
            if (-not $entryNames.Add($entry.FullName)) { throw 'Duplicate official ZIP entry' }
        }
        New-Item -ItemType Directory -Path $output | Out-Null
        # Only two exact, bounded entries are copied to fixed leaves. Never
        # extract arbitrary archive paths, PDBs, SDKs, executables or scripts.
        foreach ($leaf in @('LeviLamina.dll','manifest.json')) {
            $entry = $zip.GetEntry('LeviLamina/' + $leaf)
            $maximum = if ($leaf -ceq 'manifest.json') { 65536 } else { 16777216 }
            if (-not $entry -or $entry.Length -le 0 -or $entry.Length -gt $maximum) { throw "Missing/oversized release entry: $leaf" }
            $source = $entry.Open()
            try {
                $destination = [IO.File]::Open((Join-Path $output $leaf),[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
                try { $source.CopyTo($destination) } finally { $destination.Dispose() }
            } finally { $source.Dispose() }
        }
        $archiveEntries = @($zip.Entries | ForEach-Object { [pscustomobject]@{name=$_.FullName;size=$_.Length} })
    } finally { $zip.Dispose() }
} finally { $archiveStream.Dispose() }
$manifest = Get-Content -Raw -LiteralPath (Join-Path $output 'manifest.json') | ConvertFrom-Json
Assert-BdsServerReleaseManifest $manifest $profile
$serverDll = Join-Path $output 'LeviLamina.dll'
$serverPe = @(& $reader --file-headers --coff-exports --coff-imports $serverDll)
if ($LASTEXITCODE -ne 0) { throw 'Server PE inspection failed' }
$serverExportCount = @(Get-BdsPeNamedExports $serverPe).Count
$modHash = $null; $resolution = $null
if ($modDll) {
    $modHash = (Get-FileHash -LiteralPath $modDll -Algorithm SHA256).Hash
    $modPe = @(& $reader --file-headers --coff-exports --coff-imports $modDll)
    if ($LASTEXITCODE -ne 0) { throw 'Mod PE inspection failed' }
    $resolution = Test-BdsServerImportResolution $serverPe $modPe
    if ((Get-FileHash -LiteralPath $modDll -Algorithm SHA256).Hash -ine $modHash) { throw 'Mod DLL changed during inspection' }
    $modPe | Set-Content -LiteralPath (Join-Path $output 'mod-pe.txt') -Encoding UTF8
}
$serverPe | Set-Content -LiteralPath (Join-Path $output 'server-pe.txt') -Encoding UTF8
$evidencePath = Join-Path $output 'inspection-evidence.json'
[ordered]@{
    inspected_utc=[DateTime]::UtcNow.ToString('o')
    official_server_archive_verified=$true;server_manifest_matches=$true
    archive=$archive;archive_sha256=$archiveHash;archive_entries=$archiveEntries
    bds_profile_sha256=(Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
    server_dll=$serverDll;server_dll_sha256=(Get-FileHash -LiteralPath $serverDll -Algorithm SHA256).Hash
    server_named_export_count=$serverExportCount
    mod_dll=$modDll;mod_dll_sha256=$modHash;resolution=$resolution
    server_dependencies=Get-BdsPeDependencyModules $serverPe
    matched_server_sdk_verified=$false;packaged=$false;launch_authorized=$false;bds_load_verified=$false
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $evidencePath -Encoding UTF8
if ($PassThru) { return [pscustomobject]@{evidence_path=$evidencePath} }
if ($modDll) { "Official server release static inspection: PASS; $($resolution.mod_ll_import_count) imports resolve; NOT a BDS load proof" }
else { 'Official server release static inspection: PASS; loader only, no mod/BDS verification' }
"Evidence: $(Join-Path $output 'inspection-evidence.json')"
