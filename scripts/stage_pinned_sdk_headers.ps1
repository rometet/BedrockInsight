param(
    [string]$CatalogPath,
    [switch]$ValidateOnly,
    [switch]$NormalizeImportedNewline,
    [string[]]$ReferenceRoots = @(
        'C:\Dev\LL2660-MCAPI-Official-Audit-20261102',
        'C:\Dev\LeviLamina-26.60-Assistant-20261029',
        'C:\Dev\LL2660-NormalHooks-Lab-20261031')
)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$snapshotRoot = Join-Path $projectRoot '.build/pinned-sdk-headers-26.60.0'
$outputRoot = Join-Path $snapshotRoot 'source'
if (-not $CatalogPath) { $CatalogPath = Join-Path $snapshotRoot 'catalog.json' }
$profile = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/bds-profile.json') | ConvertFrom-Json
$catalog = Get-Content -Raw -LiteralPath $CatalogPath | ConvertFrom-Json
if ($catalog.schema_version -ne 1 -or $catalog.source_commit -cne $profile.levilamina_commit) {
    throw 'Header catalog does not match the pinned official release'
}
if (-not $ValidateOnly -and
    (Get-FileHash -LiteralPath $CatalogPath -Algorithm SHA256).Hash -ine $profile.header_catalog_sha256) {
    throw 'Header catalog digest does not match the pinned official catalog'
}

function Get-GitBlobHash([byte[]]$Bytes) {
    $prefix = [Text.Encoding]::ASCII.GetBytes("blob $($Bytes.Length)`0")
    $digest = [Security.Cryptography.SHA1]::Create()
    try {
        [void]$digest.TransformBlock($prefix,0,$prefix.Length,$prefix,0)
        [void]$digest.TransformFinalBlock($Bytes,0,$Bytes.Length)
        return [BitConverter]::ToString($digest.Hash).Replace('-','').ToLowerInvariant()
    } finally { $digest.Dispose() }
}
function Get-GitTextBlobHash([string]$Path) {
    # Git text checkouts may use CRLF. Compare canonical LF UTF-8 content to
    # the official blob, preserving BOMs and all other bytes. Never invoke
    # Git: partial-clone lazy fetch can mutate the read-only reference tree.
    $utf8 = [Text.UTF8Encoding]::new($false,$true)
    $content = $utf8.GetString([IO.File]::ReadAllBytes($Path)).Replace("`r`n","`n")
    return Get-GitBlobHash ($utf8.GetBytes($content))
}

$checkedParents = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
function Assert-LocalDestination([string]$Path) {
    $current = [IO.Path]::GetFullPath($Path)
    if (-not $current.StartsWith($projectRoot + '\',[StringComparison]::OrdinalIgnoreCase)) {
        throw "Destination escapes the project: $current"
    }
    if (([IO.File]::Exists($current) -or [IO.Directory]::Exists($current)) -and
        ([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing redirected destination: $current"
    }
    $current = [IO.Path]::GetDirectoryName($current)
    while ($current.Length -gt $projectRoot.Length -and $checkedParents.Add($current)) {
        if ([IO.Directory]::Exists($current) -and
            ([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Refusing redirected destination: $current"
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
}

$names = @{}
$files = @($catalog.files)
if ($files.Count -eq 0) { throw 'Empty header catalog' }
# Validate the entire catalog before any copy. No path from an SDK recipe is
# executed, no package/cache/index is modified, and no library is synthesized.
foreach ($file in $files) {
    $parts = $file.path.Split('/')
    if (@($parts | Where-Object { $_ -cnotmatch '^[A-Za-z0-9_][A-Za-z0-9_.+-]*$' -or $_.EndsWith('.') -or
        $_ -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)' }).Count -ne 0 -or
        ($file.path -notmatch '^(src|src-server)/(ll/api|mc)/.+\.h$' -and
         $file.path -cnotin @('COPYING','COPYING.LESSER','EULA.en.md','EULA.zh.md','xmake.lua','tooth.json')) -or
        $file.sha -cnotmatch '^[a-f0-9]{40}$' -or $names.ContainsKey($file.path)) {
        throw "Invalid or duplicate official header path/hash: $($file.path)"
    }
    $names[$file.path] = $true
    Assert-LocalDestination (Join-Path $outputRoot $file.path)
}
if ($ValidateOnly) { 'Official header catalog path validation: PASS'; return }

$missing = [Collections.Generic.List[object]]::new()
$copied = 0
$verified = 0
$mismatchedReferences = 0
$normalizedImports = 0
foreach ($file in $files) {
    $destination = Join-Path $outputRoot $file.path
    if ([IO.File]::Exists($destination)) {
        if ((Get-GitTextBlobHash $destination) -cne $file.sha) {
            # Text patch import adds a final LF. One upstream generated header
            # deliberately has none. This optional formatting step restores it
            # only when removing exactly that LF yields the official blob.
            $utf8 = [Text.UTF8Encoding]::new($false,$true)
            $content = $utf8.GetString([IO.File]::ReadAllBytes($destination)).Replace("`r`n","`n")
            $restored = $false
            if ($NormalizeImportedNewline -and $content.EndsWith("`n")) {
                $bytes = $utf8.GetBytes($content.Substring(0,$content.Length - 1))
                if ((Get-GitBlobHash $bytes) -ceq $file.sha) {
                    [IO.File]::WriteAllBytes($destination,$bytes)
                    if ((Get-GitTextBlobHash $destination) -cne $file.sha) { throw 'Imported newline verification failed' }
                    ++$normalizedImports
                    $restored = $true
                }
            }
            if (-not $restored) {
                throw "Existing staged file differs from the official blob; retained without overwrite: $destination"
            }
        }
        ++$verified
        continue
    }
    $found = $false
    foreach ($reference in $ReferenceRoots) {
        $candidate = Join-Path $reference $file.path
        if (-not [IO.File]::Exists($candidate)) { continue }
        try { $matches = (Get-GitTextBlobHash $candidate) -ceq $file.sha }
        catch [Text.DecoderFallbackException] { $matches = $false }
        if (-not $matches) { ++$mismatchedReferences; continue }
        [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
        [IO.File]::Copy($candidate,$destination,$false)
        if ((Get-GitTextBlobHash $destination) -cne $file.sha) { throw "Staged file verification failed: $destination" }
        ++$copied
        ++$verified
        $found = $true
        break
    }
    if (-not $found) { $missing.Add($file) }
    if (($verified + $missing.Count) % 2000 -eq 0) {
        Write-Output "Header staging: $verified verified, $($missing.Count) unavailable"
    }
}
Assert-LocalDestination (Join-Path $snapshotRoot 'missing.json')
Assert-LocalDestination (Join-Path $snapshotRoot 'headers-evidence.json')
New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
ConvertTo-Json -InputObject @($missing.ToArray()) -Depth 4 |
    Set-Content -LiteralPath (Join-Path $snapshotRoot 'missing.json') -Encoding UTF8
[ordered]@{
    header_snapshot_only = $true
    source_commit = $catalog.source_commit
    catalog_sha256 = (Get-FileHash -LiteralPath $CatalogPath -Algorithm SHA256).Hash
    catalog_files = $files.Count
    verified_files = $verified
    copied_this_run = $copied
    mismatched_reference_candidates = $mismatchedReferences
    normalized_import_final_newlines = $normalizedImports
    missing_files = $missing.Count
    complete_header_snapshot = ($missing.Count -eq 0)
    server_library_verified = $false
    bds_load_verified = $false
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $snapshotRoot 'headers-evidence.json') -Encoding UTF8
Write-Output "Official header staging: verified=$verified missing=$($missing.Count) copied=$copied"
Write-Output 'HEADERS ONLY: no server library, runtime data, DLL package or BDS load proof is supplied.'
