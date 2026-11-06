param([Parameter(Mandatory)][string]$SandboxRoot)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
try {
    # No process launch, port bind, file creation/copy or server modification.
    $sandbox = Assert-BdsSandboxPath $projectRoot $SandboxRoot
    Assert-BdsUnredirectedTree $sandbox
    $profilePath = Join-Path $projectRoot 'packaging/bds-profile.json'
    $profile = Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
    $tooth = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/tooth.json') | ConvertFrom-Json
    $evidencePath = Join-Path $projectRoot '.build/levilamina/release/build-evidence.json'
    Assert-BdsUnredirectedAncestors $evidencePath
    # Never substitute the historical client compile probe for release evidence.
    if (-not [IO.File]::Exists($evidencePath)) { throw 'Missing normal server release build evidence; compile probes cannot pass' }
    $modRoot = Join-Path $sandbox 'mods/BedrockInsight'
    foreach ($relative in @('bedrock_server.exe','PreLoader.dll','mods/LeviLamina/LeviLamina.dll',
        'mods/BedrockInsight/BedrockInsight.dll','mods/BedrockInsight/manifest.json',
        'server.properties','allowlist.json','permissions.json')) {
        if (-not [IO.File]::Exists((Join-Path $sandbox $relative))) { throw "Missing isolation input: $relative" }
    }
    $modEntries = @(Get-ChildItem -LiteralPath (Join-Path $sandbox 'mods') -Force)
    if (@($modEntries | Where-Object { -not $_.PSIsContainer }).Count) { throw 'Unexpected file in isolation mods root' }
    $network = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties()
    $boundPorts = @($network.GetActiveUdpListeners() | ForEach-Object { $_.Port }) +
        @($network.GetActiveTcpListeners() | ForEach-Object { $_.Port })
    # Read-only process inventory. Unknown BDS ownership fails closed.
    $runningPaths = @(Get-BdsRunningExecutablePaths)
    $facts = [pscustomobject]@{
        sandbox_root = $sandbox
        build_evidence = Get-Content -Raw -LiteralPath $evidencePath | ConvertFrom-Json
        mod_manifest = Get-Content -Raw -LiteralPath (Join-Path $modRoot 'manifest.json') | ConvertFrom-Json
        actual_dll_sha256 = (Get-FileHash -LiteralPath (Join-Path $modRoot 'BedrockInsight.dll') -Algorithm SHA256).Hash
        server_properties = Get-Content -Raw -LiteralPath (Join-Path $sandbox 'server.properties')
        allowlist_empty = (Get-Content -Raw -LiteralPath (Join-Path $sandbox 'allowlist.json')) -match '^\s*\[\s*\]\s*$'
        permissions_empty = (Get-Content -Raw -LiteralPath (Join-Path $sandbox 'permissions.json')) -match '^\s*\[\s*\]\s*$'
        world_directory_exists = Test-Path -LiteralPath (Join-Path $sandbox 'worlds')
        mod_directories = @($modEntries | ForEach-Object { $_.Name })
        bound_ports = $boundPorts
        running_bds_paths = $runningPaths
    }
    Test-BdsIsolationCandidate $facts $profile $tooth `
        (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash $projectRoot | ConvertTo-Json -Depth 4
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 2
}
