# Build-side archive policy. No installer scripts, DLL loads or server launch.
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')

function Read-BdsJsonSnapshot([string]$Path) {
    Assert-BdsUnredirectedAncestors $Path
    $bytes=[IO.File]::ReadAllBytes($Path)
    $sha=[Security.Cryptography.SHA256]::Create()
    try { $hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose() }
    $json=[Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xfeff)
    return [pscustomobject]@{path=$Path;sha256=$hash;data=($json | ConvertFrom-Json)}
}

function Assert-BdsJsonSnapshotUnchanged($Snapshot) {
    if ((Get-FileHash -LiteralPath $Snapshot.path -Algorithm SHA256).Hash -ine $Snapshot.sha256) { throw 'JSON build input changed after snapshot' }
}

function Assert-BdsInputEntry([string]$Name, [long]$Length, [long]$Attributes,
    [ValidateRange(1,268435456)][long]$MaxLength=67108864) {
    # Canonical ZIP names only. Reject Windows aliases/devices, ADS, backslashes,
    # dot segments, trailing dots/spaces and ZIP Unix symlinks/reparse points.
    if (-not $Name -or $Name.Length -gt 240 -or $Name -match '[^A-Za-z0-9_./ -]' -or
        $Name.StartsWith('/') -or $Length -lt 0 -or $Length -gt $MaxLength -or
        ($Attributes -band 0x400) -or (($Attributes -shr 16) -band 0xf000) -eq 0xa000) {
        throw 'Unsafe official archive entry'
    }
    $parts = $Name.TrimEnd('/').Split('/')
    foreach ($part in $parts) {
        if (-not $part -or $part -in @('.','..') -or $part -match '[.\s]$' -or
            $part -match '^(?i:CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)') {
            throw 'Unsafe official archive path segment'
        }
    }
    if ($Name.EndsWith('/') -and $Length -ne 0) { throw 'Nonempty ZIP directory' }
}

function Assert-BdsInputIdentity($Pin, [long]$Length, [string]$Sha256) {
    if ($Pin.name -cnotmatch '^[A-Za-z0-9_.-]+\.zip$' -or $Pin.size -le 0 -or
        $Pin.sha256 -cnotmatch '^[a-f0-9]{64}$' -or $Length -ne $Pin.size -or $Sha256 -ine $Pin.sha256) {
        throw 'Official input size/hash mismatch'
    }
}

function Assert-BdsPrelinkOutput([string]$Name, [long]$Length) {
    if ($Name -cne 'bedrock_runtime_api.lib' -or $Length -le 0) { throw 'Missing/empty prelink runtime API library' }
}

function Assert-BdsInputPins($Pins, $Profile) {
    $versions = @{levilamina=$Profile.levilamina_version;bedrockdata=$Profile.sdk_dependencies.bedrockdata;
        prelink=$Profile.sdk_dependencies.prelink;preloader=$Profile.preloader_version;peeditor='3.9.2';crashlogger='1.4.0'}
    if ($Pins.schema_version -ne 1 -or @($Pins.archives).Count -ne $versions.Count -or
        $Pins.bds.version -cne $Profile.bds_version -or $Pins.bds.sha256 -cnotmatch '^[a-f0-9]{64}$') {
        throw 'Official input profile mismatch'
    }
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    foreach ($pin in $Pins.archives) {
        if (@($versions.Keys) -cnotcontains $pin.id -or -not $seen.Add($pin.id) -or $pin.version -cne $versions[$pin.id] -or
            @($pin.files).Count -eq 0) { throw 'Duplicate, missing or mismatched official input pin' }
        Assert-BdsInputIdentity $pin $pin.size $pin.sha256
        $files = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($file in $pin.files) {
            Assert-BdsInputEntry $file 0 0
            if ($file.EndsWith('/') -or -not $files.Add($file)) { throw 'Invalid selected official payload' }
        }
        foreach ($existing in @($Profile.downloads | Where-Object { $_.name -ceq $pin.name })) {
            if ($existing.sha256 -ine $pin.sha256 -or $existing.size -ne $pin.size) { throw 'Conflicting profile archive identity' }
        }
    }
}

function Get-BdsRegularModuleImports([string[]]$Inspection, [string]$Module) {
    # Extract only a named regular import block. Other modules may legitimately
    # contain ordinals; delay-import nesting must never be mistaken for this one.
    $inside=$false; $name=$null; $symbols=@(); $found=@(); $count=0
    foreach ($line in $Inspection) {
        if ($line -ceq 'Import {') {
            if ($inside) { throw 'Malformed regular import record' }
            $inside=$true; $name=$null; $symbols=@()
        } elseif ($inside -and $line -match '^  Name: (.*)$') {
            if ($null -ne $name) { throw 'Duplicate import module field' }; $name=$Matches[1]
        } elseif ($inside -and $line -match '^  Symbol: (.*)$') {
            $symbols += $Matches[1]
        } elseif ($inside -and $line -ceq '}') {
            if (-not $name) { throw 'Missing import module field' }
            if ($name -ieq $Module) {
                $count++
                foreach ($symbol in $symbols) {
                    if ($symbol -notmatch '^(\S+) \([0-9]+\)$') { throw 'Unsupported named runtime import' }
                    $found += $Matches[1]
                }
            }
            $inside=$false
        }
    }
    if ($inside -or $count -ne 1 -or $found.Count -eq 0 -or @($found | Select-Object -Unique).Count -ne $found.Count) {
        throw 'Missing, duplicate or incomplete runtime import module'
    }
    return $found
}
