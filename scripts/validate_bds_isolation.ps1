# Candidate policy only. Passing this is NOT permission to launch BDS.
. (Join-Path $PSScriptRoot 'validate_bds_profile.ps1')
. (Join-Path $PSScriptRoot 'capture_access_policy.ps1')

function Assert-BdsSandboxPath([string]$ProjectRoot, [string]$SandboxRoot) {
    $project = [IO.Path]::GetFullPath($ProjectRoot).TrimEnd('\')
    $sandbox = [IO.Path]::GetFullPath($SandboxRoot).TrimEnd('\')
    $prefix = $project + '\.build\isolated-bds\'
    if (-not $sandbox.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
        $sandbox.Substring($prefix.Length) -cnotmatch '^run-[a-f0-9]{32}$') {
        throw 'Isolation root must be a unique project-local .build/isolated-bds/run-<32 lowercase hex> directory'
    }
    return $sandbox
}

function Assert-BdsUnredirectedAncestors([string]$Path) {
    $ancestor = [IO.Path]::GetFullPath($Path)
    while ($ancestor) {
        if (([IO.File]::Exists($ancestor) -or [IO.Directory]::Exists($ancestor)) -and
            ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Redirected isolation path: $ancestor"
        }
        $ancestor = [IO.Path]::GetDirectoryName($ancestor)
    }
}

function Assert-BdsUnredirectedTree([string]$SandboxRoot) {
    # Check ancestors too. Never recurse through a junction/symlink first and
    # inspect it afterwards. This function reads only; it creates no paths.
    Assert-BdsUnredirectedAncestors $SandboxRoot
    if (-not [IO.Directory]::Exists($SandboxRoot)) { throw 'Missing isolation directory' }
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($SandboxRoot)
    while ($pending.Count) {
        foreach ($entry in [IO.Directory]::EnumerateFileSystemEntries($pending.Pop())) {
            $attributes = [IO.File]::GetAttributes($entry)
            if ($attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Redirected isolation path: $entry"
            }
            if ($attributes -band [IO.FileAttributes]::Directory) { $pending.Push($entry) }
        }
    }
}

function Read-BdsIsolationProperties([string]$Text) {
    $properties = @{}
    foreach ($line in $Text -split '\r?\n') {
        if (-not $line.Trim() -or $line.TrimStart().StartsWith('#')) { continue }
        if ($line -notmatch '^\s*([a-z][a-z0-9-]*)\s*=(.*)$') { throw 'Invalid server.properties syntax' }
        $key = $Matches[1]
        if ($properties.ContainsKey($key)) { throw "Duplicate server.properties key: $key" }
        $properties[$key] = $Matches[2].Trim()
    }
    return $properties
}

function Assert-BdsProcessExecutablePath([object]$Path) {
    # MainModule normally returns a DOS absolute path. Do not guess ownership
    # from relative, device/UNC, short-name or lexically ambiguous spellings.
    if ($Path -isnot [string] -or $Path -notmatch '^[a-zA-Z]:\\' -or
        $Path.Substring(3) -match '[<>:"/|?*~\x00-\x1f]' -or
        @($Path.Substring(3).Split('\') | Where-Object {
            -not $_ -or $_ -match '[.\s]$'
        }).Count -ne 0 -or
        [IO.Path]::GetFullPath($Path) -ine $Path) {
        throw 'Cannot resolve a running BDS executable path: non-canonical or missing path'
    }
    return $Path
}

function Get-BdsProcessExecutablePath {
    param([Parameter(Mandatory)]$Process,
          [Parameter(Mandatory)][ValidateSet('bedrock_server','bedrock_server_mod')][string]$ExpectedName)
    try {
        $path = Assert-BdsProcessExecutablePath $Process.MainModule.FileName
        if ([IO.Path]::GetFileName($path) -ine ($ExpectedName + '.exe')) {
            throw 'Unexpected executable name'
        }
        return $path
    } catch {
        throw "Cannot resolve a running BDS executable path: $($_.Exception.Message)"
    }
}

function Get-BdsRunningExecutablePaths {
    # Read-only OS process metadata, not WMI/CIM and not a launch/stop API.
    # Collect the complete result before emitting anything. Any enumeration
    # or MainModule error invalidates the whole inventory; this is a snapshot,
    # not proof that a process remains alive or unchanged after observation.
    $paths = [Collections.Generic.List[string]]::new()
    foreach ($name in @('bedrock_server','bedrock_server_mod')) {
        $processes = @()
        try {
            $processes = @([Diagnostics.Process]::GetProcessesByName($name))
            foreach ($process in $processes) {
                $paths.Add((Get-BdsProcessExecutablePath $process $name))
            }
        } catch {
            throw "Cannot resolve a running BDS executable path or inventory: $($_.Exception.Message)"
        } finally {
            # Dispose releases only our local metadata handles; it does not
            # signal, wait for, terminate or otherwise modify a server.
            foreach ($process in $processes) { $process.Dispose() }
        }
    }
    return $paths.ToArray()
}

function Test-BdsIsolationCandidate {
    param([Parameter(Mandatory)]$Facts,
          [Parameter(Mandatory)]$Profile,
          [Parameter(Mandatory)]$Tooth,
          [Parameter(Mandatory)][string]$ProfileSha256,
          [Parameter(Mandatory)][string]$ProjectRoot)
    $sandbox = Assert-BdsSandboxPath $ProjectRoot $Facts.sandbox_root
    $runId = [IO.Path]::GetFileName($sandbox).Substring(4)
    $evidence = $Facts.build_evidence
    foreach ($flag in @('compile_probe','sdk_is_server','sdk_version_matches_2660')) {
        if ($evidence.$flag -isnot [bool]) { throw "Missing/invalid release evidence flag: $flag" }
    }
    if ($evidence.compile_probe -or -not $evidence.sdk_is_server -or -not $evidence.sdk_version_matches_2660 -or
        $evidence.bds_profile_sha256 -ine $ProfileSha256 -or
        $evidence.bds_profile.levilamina_commit -cne $Profile.levilamina_commit -or
        $evidence.bds_profile.bds_version -cne $Profile.bds_version) {
        throw 'Release evidence is not for the current pinned server profile'
    }
    if ($evidence.dll_sha256 -notmatch '^[a-fA-F0-9]{64}$' -or
        $Facts.actual_dll_sha256 -ine $evidence.dll_sha256) { throw 'Staged mod DLL hash mismatch' }
    Assert-BdsProfileInputs $evidence.sdk_dependency_inputs $Profile $Facts.mod_manifest $Tooth
    return Test-BdsIsolationBoundary $Facts $ProjectRoot
}

function Test-BdsIsolationBoundary($Facts, [string]$ProjectRoot, $RealPlayerAccess=$null) {
    # Shared physical/configuration boundary. This contains no SDK/release
    # assertions: each caller must independently validate its build provenance.
    $sandbox = Assert-BdsSandboxPath $ProjectRoot $Facts.sandbox_root
    $runId = [IO.Path]::GetFileName($sandbox).Substring(4)
    if ($null -ne $RealPlayerAccess) {
        Assert-BdsCapturePlayerAllowlist $Facts.allowlist_text $RealPlayerAccess
        if ($Facts.allowlist_empty -isnot [bool] -or $Facts.allowlist_empty) { throw 'Real-player allowlist metadata mismatch' }
    } elseif ($Facts.allowlist_empty -isnot [bool] -or -not $Facts.allowlist_empty) { throw 'Isolation requires allowlist_empty' }
    foreach ($flag in @('permissions_empty')) {
        if ($Facts.$flag -isnot [bool] -or -not $Facts.$flag) { throw "Isolation requires $flag" }
    }
    if ($Facts.world_directory_exists -isnot [bool] -or $Facts.world_directory_exists) {
        throw 'Isolation requires a fresh world; worlds directory must not exist'
    }
    if (@($Facts.mod_directories).Count -ne 2 -or
        @($Facts.mod_directories | Where-Object { $_ -cnotin @('BedrockInsight','LeviLamina') }).Count -ne 0 -or
        @($Facts.mod_directories | Select-Object -Unique).Count -ne 2) {
        throw 'Unexpected mod directories in minimal isolation candidate'
    }
    $properties = Read-BdsIsolationProperties $Facts.server_properties
    $required = @{'transport'='nethernet';'enable-lan-visibility'='false';'allow-list'='true';'online-mode'='true';
        'max-players'='1';'default-player-permission-level'='visitor';'allow-cheats'='false';
        'level-name'="insight-isolated-$runId"}
    foreach ($key in $required.Keys) {
        if ($properties[$key] -cne $required[$key]) { throw "Unsafe isolation property: $key" }
    }
    $allowedKeys = @($required.Keys) + @('server-name','server-port','server-portv6','server-udp-ports','view-distance','tick-distance')
    foreach ($key in $properties.Keys) {
        if ($key -cnotin $allowedKeys) { throw "Unreviewed isolation property: $key" }
    }
    # NetherNet signaling uses server-port (TCP) and gameplay uses an explicit UDP port.
    # Keep the one-player isolated test on its verified, non-default high port.
    if ($properties['server-udp-ports'] -cne $properties['server-port']) {
        throw 'Unsafe isolation property: server-udp-ports'
    }
    $ports = @()
    foreach ($key in @('server-port','server-portv6')) {
        $value = $properties[$key]
        if ($value -notmatch '^[0-9]{1,5}$' -or [int]$value -lt 49152 -or [int]$value -gt 65535) {
            throw "Isolation requires distinct high ports: $key"
        }
        $port = [int]$value
        if ($ports -contains $port) { throw 'Isolation ports must be distinct' }
        if (@($Facts.bound_ports) -contains $port) { throw "Isolation port already in use: $port" }
        $ports += $port
    }
    foreach ($path in @($Facts.running_bds_paths)) {
        $path = Assert-BdsProcessExecutablePath $path
        if ($path.StartsWith($sandbox + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Isolation candidate already has a running BDS process'
        }
    }
    return [pscustomobject]@{
        candidate_policy_passed = $true
        sandbox_root = $sandbox
        world_name = $properties['level-name']
        checked_ports = $ports
        launch_authorized = $false
        bds_load_verified = $false
        limits = 'Read-only candidate checks only: no file-identity/alias/ACL, complete runtime binary provenance, process/port race exclusion, port reservation or BDS/LL execution proof. Recheck before any launch.'
    }
}
