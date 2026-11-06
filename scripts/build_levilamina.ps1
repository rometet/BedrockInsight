param([switch]$CompileProbe)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
try {
    $profilePath = Join-Path $projectRoot 'packaging/bds-profile.json'
    $profile = Get-Content -Raw -LiteralPath $profilePath | ConvertFrom-Json
    foreach ($name in @('BEDROCKINSIGHT_CLANGCL_ROOT','BEDROCKINSIGHT_LL_ROOT','BEDROCKINSIGHT_LL_LIB',
        'BEDROCKINSIGHT_SQLITE_ROOT','BEDROCKINSIGHT_XMAKE_PACKAGES')) {
        if (-not [Environment]::GetEnvironmentVariable($name)) { throw "$name is required" }
    }
    $sdkRoot = [IO.Path]::GetFullPath($env:BEDROCKINSIGHT_LL_ROOT)
    $sdkLib = [IO.Path]::GetFullPath($env:BEDROCKINSIGHT_LL_LIB)
    if (-not $sdkLib.StartsWith($sdkRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'LeviLamina.lib must belong to the selected SDK root'
    }
    $manifestPath = Join-Path $sdkRoot 'manifest.txt'
    $configPath = Join-Path $sdkRoot '.xmake\windows\x64\xmake.conf'
    $evidence = if (Test-Path -LiteralPath $manifestPath) { Get-Content -Raw -LiteralPath $manifestPath }
        elseif (Test-Path -LiteralPath $configPath) { Get-Content -Raw -LiteralPath $configPath }
        else { '' }
    $server = $evidence -match 'target_type\s*=\s*"server"'
    # A checkout's tooth.json is a source declaration, not binary provenance.
    $versionText = if (Test-Path -LiteralPath $manifestPath) { $evidence }
        elseif (Test-Path -LiteralPath (Join-Path $sdkRoot 'bin\LeviLamina\manifest.json')) {
            Get-Content -Raw -LiteralPath (Join-Path $sdkRoot 'bin\LeviLamina\manifest.json')
        } else { '' }
    $versionMatches = if (Test-Path -LiteralPath $manifestPath) {
        # Only the top-level package version, not a dependency version.
        $versionText -match ('(?m)^    version = "' + [regex]::Escape($profile.levilamina_version) + '"')
    } else { $versionText -match ('"version"\s*:\s*"' + [regex]::Escape($profile.levilamina_version) + '"') }
    if ((-not $server -or -not $versionMatches) -and -not $CompileProbe) {
        throw 'SDK mismatch: requires proven LeviLamina 26.60 server build/package; client or 26.51 inputs cannot produce a BDS package. Use -CompileProbe only for non-loadable link diagnostics.'
    }
    $headerRoot = if (Test-Path -LiteralPath (Join-Path $sdkRoot 'include\ll\api\mod\RegisterHelper.h')) {
        Join-Path $sdkRoot 'include'
    } else { Join-Path $sdkRoot 'src' }
    $compiler = Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
    $sqliteLib = Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib\sqlite3.lib'
    foreach ($required in @($compiler,$sdkLib,$sqliteLib,(Join-Path $headerRoot 'mc\world\actor\player\Player.h'))) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing build input: $required" }
    }
    $buildRoot = Join-Path $projectRoot $(if ($CompileProbe) { '.build\compile-probe' } else { '.build\levilamina\release' })
    New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
    $dll = Join-Path $buildRoot 'BedrockInsight.dll'
    $pdb = Join-Path $buildRoot 'BedrockInsight.pdb'
    $arguments = @('/nologo','/LD','/EHsc','/std:c++20','/Zc:__cplusplus','/permissive-','/MD','/O2',
        '/W3','/utf-8','/DNOMINMAX','/DLL_PLAT_S','-Wno-microsoft-include','-Wno-invalid-constexpr',
        "-I$(Join-Path $projectRoot 'src')","-I$headerRoot",
        "-I$(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'include')")
    if (Test-Path -LiteralPath (Join-Path $sdkRoot 'src-server')) {
        $arguments += "-I$(Join-Path $sdkRoot 'src-server')"
    }
    . (Join-Path $PSScriptRoot 'resolve_sdk_inputs.ps1')
    $sdkInputs = Resolve-SdkInputs -SdkRoot $sdkRoot -PackagesRoot $env:BEDROCKINSIGHT_XMAKE_PACKAGES
    if (-not $CompileProbe) {
        . (Join-Path $PSScriptRoot 'validate_bds_profile.ps1')
        Assert-BdsProfileInputs -SdkInputs $sdkInputs -Profile $profile `
            -NativeManifest (Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'manifest.json') | ConvertFrom-Json) `
            -Tooth (Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/tooth.json') | ConvertFrom-Json)
    }
    foreach ($directory in $sdkInputs.include_dirs) { $arguments += "-I$directory" }
    foreach ($define in $sdkInputs.defines) { $arguments += "/D$define" }
    $sources = @('levilamina/mod.cpp','levilamina/runtime.cpp','levilamina/busy_tick.cpp','core/admin_api.cpp','core/event.cpp',
        'core/rollback.cpp','core/sqlite_api.cpp','core/storage.cpp')
    foreach ($source in $sources) { $arguments += Join-Path $projectRoot "src/bedrock_insight/$source" }
    $arguments += @("/Fe:$dll","/Fd:$pdb",'/link','/DEBUG',"/IMPLIB:$(Join-Path $buildRoot 'BedrockInsight.lib')",$sdkLib,$sqliteLib)
    # All generated compiler output, including temporary objects, stays here.
    New-Item -ItemType Directory -Path (Join-Path $buildRoot 'tmp') -Force | Out-Null
    $env:TEMP = Join-Path $buildRoot 'tmp'
    $env:TMP = $env:TEMP
    Push-Location -LiteralPath $buildRoot
    try {
        & $compiler @arguments
        if ($LASTEXITCODE -ne 0) { throw "Native compile/link failed: $LASTEXITCODE" }
    } finally { Pop-Location }
    $inspection = & (Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe') --coff-exports $dll
    if ($LASTEXITCODE -ne 0 -or -not ($inspection -match '^\s*Name: ll_mod_load$') -or
        -not ($inspection -match '^\s*Name: ll_memory_operator_overrided$')) {
        throw 'Native DLL must export ll_mod_load and the SDK unified-allocation marker ll_memory_operator_overrided'
    }
    [ordered]@{
        compile_probe = [bool]$CompileProbe
        sdk_root = $sdkRoot
        sdk_is_server = $server
        sdk_version_matches_2660 = $versionMatches
        sdk_import_library = $sdkLib
        sdk_import_sha256 = (Get-FileHash -LiteralPath $sdkLib -Algorithm SHA256).Hash
        sdk_dependency_inputs = $sdkInputs
        bds_profile = $profile
        bds_profile_sha256 = (Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
        compiler = $compiler
        arguments = $arguments
        required_exports = @('ll_mod_load','ll_memory_operator_overrided')
        dll_sha256 = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
        bds_load_verified = $false
    } | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $buildRoot 'build-evidence.json')
    if ($CompileProbe) {
        Write-Output 'COMPILE/LINK PROBE PASS: SDK UNVERIFIED; NOT A BDS LOADABLE PACKAGE'
        Get-FileHash -LiteralPath $dll -Algorithm SHA256 | Format-List
        exit 0
    }
    $packageRoot = Join-Path $projectRoot 'dist\BedrockInsight-server'
    $packageMod = Join-Path $packageRoot 'BedrockInsight'
    New-Item -ItemType Directory -Path $packageMod -Force | Out-Null
    Copy-Item -LiteralPath $dll -Destination $packageMod
    Copy-Item -LiteralPath $pdb -Destination $packageMod
    Copy-Item -LiteralPath (Join-Path $projectRoot 'manifest.json') -Destination $packageMod
    Copy-Item -LiteralPath (Join-Path $projectRoot 'packaging\tooth.json') -Destination $packageRoot
    $zip = Join-Path $projectRoot 'dist\BedrockInsight-server-win-x64.zip'
    # Placements in tooth.json are relative to the ZIP root.
    & tar.exe -a -c -f $zip -C $packageRoot BedrockInsight tooth.json
    if ($LASTEXITCODE -ne 0) { throw 'Package ZIP failed' }
    Write-Output 'NATIVE BUILD/PACKAGE PASS: isolated BDS load still required'
    Get-FileHash -LiteralPath $dll,$zip -Algorithm SHA256 | Format-List
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 2
}
