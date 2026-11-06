# Extract verified official inputs to separate build-side roots, never a BDS root.
param([switch]$PassThru)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$pinsPath=Join-Path $project 'packaging/official-inputs.json'
$profilePath=Join-Path $project 'packaging/bds-profile.json'
$pinsSnapshot=Read-BdsJsonSnapshot $pinsPath; $pins=$pinsSnapshot.data
$profileSnapshot=Read-BdsJsonSnapshot $profilePath; $profile=$profileSnapshot.data
Assert-BdsInputPins $pins $profile
$root=Join-Path $project ('.build/runtime-inputs/run-' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $root
New-Item -ItemType Directory -Path $root | Out-Null
$archives=@()
foreach ($pin in $pins.archives) {
    $source=Join-Path $project ('.build/bds-downloads/' + $pin.name)
    Assert-BdsUnredirectedAncestors $source
    $stream=[IO.File]::Open($source,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    try {
        $sha=[Security.Cryptography.SHA256]::Create()
        try { $hash=([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
        finally { $sha.Dispose() }
        Assert-BdsInputIdentity $pin $stream.Length $hash
        $stream.Position=0
        $zip=[IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Read,$true)
        try {
            if ($zip.Entries.Count -gt 128) { throw 'Excessive official ZIP entry count' }
            $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            $total=0L
            foreach ($entry in $zip.Entries) {
                Assert-BdsInputEntry $entry.FullName $entry.Length $entry.ExternalAttributes
                if (-not $names.Add($entry.FullName)) { throw 'Duplicate official ZIP entry' }
                $total+=$entry.Length
                if ($total -gt 134217728) { throw 'Excessive official ZIP expanded size' }
            }
            $payloads=@()
            foreach ($selected in $pin.files) {
                $entry=$zip.GetEntry($selected)
                if ($null -eq $entry -or $entry.Length -eq 0) { throw "Missing official payload: $selected" }
                $target=Join-Path (Join-Path $root $pin.id) $selected
                Assert-BdsUnredirectedAncestors $target
                New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($target)) -Force | Out-Null
                $payloadStream=$entry.Open()
                try {
                    $output=[IO.File]::Open($target,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
                    try {
                        $buffer=New-Object byte[] 65536; $written=0L
                        while (($read=$payloadStream.Read($buffer,0,$buffer.Length)) -gt 0) {
                            $written+=$read
                            if ($written -gt $entry.Length) { throw 'Official payload exceeds declared size' }
                            $output.Write($buffer,0,$read)
                        }
                        if ($written -ne $entry.Length) { throw 'Truncated official payload' }
                    } finally { $output.Dispose() }
                } finally { $payloadStream.Dispose() }
                $payloads += [pscustomobject]@{entry=$selected;path=$target;size=$entry.Length;
                    sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash}
            }
            $archives += [pscustomobject]@{id=$pin.id;version=$pin.version;source=$source;archive_sha256=$hash;
                archive_size=$stream.Length;selected_files=$payloads;unselected_entries=@($zip.Entries.FullName | Where-Object { $_ -cnotin $pin.files })}
        } finally { $zip.Dispose() }
    } finally { $stream.Dispose() }
}
$manifest=Get-Content -Raw -LiteralPath (Join-Path $root 'levilamina/LeviLamina/manifest.json') | ConvertFrom-Json
. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
Assert-BdsServerReleaseManifest $manifest $profile
Assert-BdsUnredirectedTree $root
Assert-BdsJsonSnapshotUnchanged $pinsSnapshot
Assert-BdsJsonSnapshotUnchanged $profileSnapshot
$evidence=Join-Path $root 'staging-evidence.json'
[pscustomobject]@{staged_utc=[DateTime]::UtcNow.ToString('o');root=$root;
    pins_sha256=$pinsSnapshot.sha256;bds_profile_sha256=$profileSnapshot.sha256;
    official_archives_verified=$true;archives=$archives;
    tool_executed=$false;installer_executed=$false;runtime_dependencies_verified=$false;
    launch_authorized=$false;bds_load_verified=$false;
    limits='Build-side selected payload staging only. No BDS files, localization, scripts, installation or executable/DLL loading. Hash provenance is the coordinator handoff, not a new upstream fetch.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $evidence -Encoding UTF8
if ($PassThru) { [pscustomobject]@{root=$root;evidence_path=$evidence} }
else { "Official input staging PASS (six pinned archives; no execution). Evidence: $evidence" }
