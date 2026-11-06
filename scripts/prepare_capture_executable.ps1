# Patch only the authenticated BDS COPY in a checked, never-started scratch run.
param([Parameter(Mandatory)][string]$SandboxRoot,
      [switch]$RealPlayer, [string]$PlayerName, [string]$PlayerXuid)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Runtime.Serialization
$check=& (Join-Path $PSScriptRoot 'check_capture_isolation.ps1') -SandboxRoot $SandboxRoot -PassThru `
    -RealPlayer:$RealPlayer -PlayerName $PlayerName -PlayerXuid $PlayerXuid
if (-not $check.candidate_policy_passed) { throw 'Capture isolation policy failed' }
$root=$check.sandbox_root
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$reader=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
if (-not [IO.File]::Exists($reader)) { throw 'Missing LLVM reader' }
$original=Join-Path $root 'bedrock_server.exe'
$editor=Join-Path $root 'PeEditor.exe'
$originalHash=(Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash
$editorHash=(Get-FileHash -LiteralPath $editor -Algorithm SHA256).Hash
# Only localization DATA from the coordinator's official tagged source ZIP.
# This observed digest is not claimed to be an upstream published checksum.
$loc=Join-Path $project '.build/bds-downloads/levilamina-loc-v1.7.0-source.zip'
$locStream=[IO.File]::Open($loc,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $locHash=([BitConverter]::ToString($sha.ComputeHash($locStream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
    if ($locStream.Length -ne 52070 -or $locHash -cne '32448f019465830b343d9135d460686dbe6cdb512fdfe117b89de89188239d88') { throw 'Localization source identity mismatch' }
    $locStream.Position=0
    $zip=[IO.Compression.ZipArchive]::new($locStream,[IO.Compression.ZipArchiveMode]::Read,$true)
    try {
        $matches=@($zip.Entries | Where-Object { $_.FullName -ceq 'levilamina-loc-1.7.0/lang/en.json' })
        if ($matches.Count -ne 1 -or $matches[0].Length -ne 8116) { throw 'Unexpected localization payload' }
        $stream=$matches[0].Open(); $textReader=[IO.StreamReader]::new($stream)
        try {
            $json=$textReader.ReadToEnd()
            # The official locale legitimately uses an empty-string key, which
            # PS5 ConvertFrom-Json cannot materialize as a PSCustomObject.
            $jsonReader=[Runtime.Serialization.Json.JsonReaderWriterFactory]::CreateJsonReader(
                [Text.Encoding]::UTF8.GetBytes($json),[Xml.XmlDictionaryReaderQuotas]::Max)
            try { while ($jsonReader.Read()) {} } finally { $jsonReader.Dispose() }
        }
        finally { $textReader.Dispose(); $stream.Dispose() }
        New-Item -ItemType Directory -Path (Join-Path $root 'mods/LeviLamina/lang') | Out-Null
        $json | Set-Content -LiteralPath (Join-Path $root 'mods/LeviLamina/lang/en.json') -Encoding UTF8
    } finally { $zip.Dispose() }
} finally { $locStream.Dispose() }
Push-Location -LiteralPath $root
try {
    # -m only: NO -b/--inplace; the original EXE cannot be renamed by this CLI.
    Invoke-BdsLoggedNative $editor @('-m','--exe',$original,'-o',$root) (Join-Path $root 'PeEditor-build.log')
} finally { Pop-Location }
if ((Get-FileHash -LiteralPath $original -Algorithm SHA256).Hash -ine $originalHash -or
    (Get-FileHash -LiteralPath $editor -Algorithm SHA256).Hash -ine $editorHash) { throw 'PeEditor changed an authenticated input' }
$modified=Join-Path $root 'bedrock_server_mod.exe'
Invoke-BdsLoggedNative $reader @('--file-headers','--coff-imports',$modified) (Join-Path $root 'patched-server-pe.txt')
$pe=@(Get-Content -LiteralPath (Join-Path $root 'patched-server-pe.txt'))
if (-not ($pe -ccontains 'Format: COFF-x86-64') -or -not ($pe -match '^  Machine: IMAGE_FILE_MACHINE_AMD64 ')) { throw 'Modified server is not AMD64 PE' }
$imports=@(Get-BdsRegularModuleImports $pe 'PreLoader.dll')
if ($imports.Count -ne 1 -or $imports[0] -cne 'pl_resolve_symbol') { throw 'Missing exact PreLoader injection' }
$files=@('bedrock_server_mod.exe','PeEditor-build.log','patched-server-pe.txt','mods/LeviLamina/lang/en.json') | ForEach-Object {
    $file=Join-Path $root $_
    [pscustomobject]@{path=$_;size=([IO.FileInfo]::new($file)).Length;sha256=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash}
}
[pscustomobject]@{prepared_utc=[DateTime]::UtcNow.ToString('o');sandbox_root=$root;
    source_exe_sha256=$originalHash;peeditor_sha256=$editorHash;files=$files;
    localization_archive_sha256=$locHash;localization_provenance='Official tagged source handoff; digest locally observed, not upstream checksum authenticated';
    preloader_injection_verified=$true;launch_authorized=$false;bds_load_verified=$false
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $root 'capture-patch-evidence.json') -Encoding UTF8
"Capture executable preparation PASS; no server launch. Root: $root"
