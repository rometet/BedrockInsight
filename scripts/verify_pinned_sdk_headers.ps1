param([string]$ReferenceSdkRoot = 'C:\Dev\LeviLamina-26.60-Assistant-20261029',
    [string]$BuildOutputRoot = '')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$snapshot = Join-Path $projectRoot '.build/pinned-sdk-headers-26.60.0'
$sourceRoot = Join-Path $snapshot 'source'
# Revalidate every staged blob without sourcing a substitute from an old SDK.
& (Join-Path $PSScriptRoot 'stage_pinned_sdk_headers.ps1') -ReferenceRoots @() -NormalizeImportedNewline
$headers = Get-Content -Raw -LiteralPath (Join-Path $snapshot 'headers-evidence.json') | ConvertFrom-Json
if (-not $headers.complete_header_snapshot) { throw 'Official header snapshot is incomplete' }
foreach ($name in @('BEDROCKINSIGHT_CLANGCL_ROOT','BEDROCKINSIGHT_SQLITE_ROOT','BEDROCKINSIGHT_XMAKE_PACKAGES')) {
    if (-not [Environment]::GetEnvironmentVariable($name)) { throw "$name is required" }
}
. (Join-Path $PSScriptRoot 'resolve_sdk_inputs.ps1')
$recorded = Resolve-SdkInputs -SdkRoot $ReferenceSdkRoot -PackagesRoot $env:BEDROCKINSIGHT_XMAKE_PACKAGES
$recipe = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot 'xmake.lua')
$declared = @{}
foreach ($match in [regex]::Matches($recipe,'add_requires\("([a-z0-9_-]+)(?:\[[^\]]+\])? ([A-Za-z0-9_.+-]+)"')) {
    $name = $match.Groups[1].Value
    $declared[$name] = @($declared[$name]) + $match.Groups[2].Value
}
if ($declared.Count -lt 10) { throw 'Unsupported pinned source dependency declarations' }
$available = @{}
$allowed = @{}
foreach ($dependency in $recorded.dependencies) {
    $available[$dependency.name] = $dependency
    if ($declared.ContainsKey($dependency.name) -and
        $declared[$dependency.name] -ccontains $dependency.version) {
        $allowed[$dependency.name] = $dependency
    }
}
# Only exact child pins of selected packages can extend the header closure.
# A child cannot override a different direct pin from the official source.
do {
    $added = $false
    foreach ($parent in @($allowed.Values)) {
        $text = Get-Content -Raw -LiteralPath (Join-Path $parent.root 'manifest.txt')
        $deps = [regex]::Match($text,'(?ms)^    deps = \{\r?\n(?<body>.*?)^    \},?\r?$')
        foreach ($entry in [regex]::Matches($deps.Groups['body'].Value,
            '(?ms)^        (?<key>[A-Za-z_][A-Za-z0-9_]*|\["[A-Za-z0-9_.+-]+"\]) = \{\r?\n(?<body>.*?)^        \},?\r?$')) {
            $name = $entry.Groups['key'].Value.Trim('[',']','"')
            if ($allowed.ContainsKey($name) -or -not $available.ContainsKey($name)) { continue }
            $child = $available[$name]
            $body = $entry.Groups['body'].Value
            $version = [regex]::Match($body,'(?m)^            version = "([^"]+)"').Groups[1].Value
            $hash = [regex]::Match($body,'(?m)^            buildhash = "([a-f0-9]{32})"').Groups[1].Value
            if ($child.version -cne $version -or $child.buildhash -cne $hash) { continue }
            if ($declared.ContainsKey($name) -and $declared[$name] -cnotcontains $version) { continue }
            $allowed[$name] = $child
            $added = $true
        }
    }
} while ($added)
foreach ($name in @('entt','fmt','parallel-hashmap','concurrentqueue','type_safe')) {
    if (-not $allowed.ContainsKey($name)) { throw "Missing exact official header dependency: $name" }
}
$includeDirs = @($recorded.include_dirs | Where-Object {
    $directory = $_
    @($allowed.Values | Where-Object {
        $directory.StartsWith($_.root + '\',[StringComparison]::OrdinalIgnoreCase)
    }).Count -gt 0
})
$buildRoot = Join-Path $projectRoot '.build/pinned-sdk-header-compile'
if($BuildOutputRoot) {
    # Candidate builds own a NEW output directory. Never share/clear objects or
    # a log that another host/Codex build may still be writing.
    $buildRoot=[IO.Path]::GetFullPath($BuildOutputRoot)
    $prefix=[IO.Path]::GetFullPath((Join-Path $projectRoot '.build'))+[IO.Path]::DirectorySeparatorChar
    if(-not $buildRoot.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Header output must stay inside the project .build directory'
    }
    . (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
    Assert-BdsUnredirectedAncestors $buildRoot
    if(Test-Path -LiteralPath $buildRoot){throw 'Header output must be NEW; do not reuse another build'}
}
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'tmp') -Force | Out-Null
$env:TEMP = Join-Path $buildRoot 'tmp'
$env:TMP = $env:TEMP
$compiler = Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$arguments = @('/nologo','/c','/EHsc','/std:c++20','/Zc:__cplusplus','/permissive-','/MD','/O2','/W3',
    '/utf-8','/DNOMINMAX','/DLL_PLAT_S','/DFMT_USE_FULL_CACHE_DRAGONBOX=1',
    '/DENTT_PACKED_PAGE=128','/DENTT_SPARSE_PAGE=4096','-Wno-microsoft-include','-Wno-invalid-constexpr',
    "-I$(Join-Path $sourceRoot 'src')","-I$(Join-Path $sourceRoot 'src-server')",
    "-I$(Join-Path $projectRoot 'src')","-I$(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'include')")
foreach ($directory in $includeDirs) { $arguments += "-I$directory" }
$compileOptions = @($arguments)
foreach ($name in @('mod.cpp','runtime.cpp','busy_tick.cpp','player_actions.cpp','player_death.cpp','player_modes.cpp','player_pickup.cpp','player_jump.cpp','player_kills.cpp','player_use.cpp','player_hurt.cpp','player_block.cpp','player_block_interaction.cpp','player_travel.cpp','player_damage.cpp','player_craft.cpp','player_trade.cpp','player_breed.cpp','player_target.cpp','stats_command.cpp')) {
    $arguments += Join-Path $projectRoot "src/bedrock_insight/levilamina/$name"
}
Push-Location -LiteralPath $buildRoot
try {
    . (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
    try { Invoke-BdsLoggedNative $compiler $arguments (Join-Path $buildRoot 'compile.log') }
    catch {
        Get-Content -LiteralPath (Join-Path $buildRoot 'compile.log') -Tail 25
        throw
    }
} finally { Pop-Location }
$objects = @('mod.obj','runtime.obj','busy_tick.obj','player_actions.obj','player_death.obj','player_modes.obj','player_pickup.obj','player_jump.obj','player_kills.obj','player_use.obj','player_hurt.obj','player_block.obj','player_block_interaction.obj','player_travel.obj','player_damage.obj','player_craft.obj','player_trade.obj','player_breed.obj','player_target.obj','stats_command.obj') | ForEach-Object {
    $path = Join-Path $buildRoot $_
    [pscustomobject]@{path=$path;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
}
[ordered]@{
    compile_only = $true
    header_source_commit = $headers.source_commit
    header_catalog_sha256 = $headers.catalog_sha256
    compiler = $compiler
    compiler_log = Join-Path $buildRoot 'compile.log'
    arguments = $arguments
    compile_options = $compileOptions
    header_dependencies = @($allowed.Values | Sort-Object name)
    objects = @($objects)
    sdk_library_linked = $false
    bds_load_verified = $false
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $buildRoot 'compile-evidence.json') -Encoding UTF8
'Official 26.60.0 server header compile: PASS (objects only; no DLL link or BDS load)'
