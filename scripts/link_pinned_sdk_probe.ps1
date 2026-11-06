# Complete native DLL diagnostic against the pinned official headers. The
# reference .lib is from an OLD CLIENT tree: this NEVER emits a server package.
param([string]$ReferenceSdkRoot = 'C:\Dev\LeviLamina-26.60-Assistant-20261029')
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
. (Join-Path $PSScriptRoot 'validate_import_metadata.ps1')
$referenceLibrary = Join-Path $ReferenceSdkRoot 'build/windows/x64/release/LeviLamina.lib'
$expectedReferenceHash = 'cd5e008b847da5da12049cecec0f4bbe2ae104126914f7d5ccebfe4d4c1bacaa'
if ((Get-FileHash -LiteralPath $referenceLibrary -Algorithm SHA256).Hash -ine $expectedReferenceHash) {
    throw 'Reference library changed; diagnostic requires the explicitly audited client import metadata'
}
$reader = Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
$libraryInspection = @(& $reader --file-headers --sections $referenceLibrary)
if ($LASTEXITCODE -ne 0) { throw 'Reference library inspection failed' }
$importMetadata = Assert-NamedLlImportMetadata $libraryInspection
$headerBuildRoot = Join-Path $projectRoot '.build/pinned-sdk-header-compile'
Assert-BdsUnredirectedAncestors $headerBuildRoot
if ([IO.Directory]::Exists($headerBuildRoot)) { Assert-BdsUnredirectedTree $headerBuildRoot }
# Always verify all official blobs and recompile the real entry/runtime first.
& (Join-Path $PSScriptRoot 'verify_pinned_sdk_headers.ps1') -ReferenceSdkRoot $ReferenceSdkRoot
$compile = Get-Content -Raw -LiteralPath (Join-Path $headerBuildRoot 'compile-evidence.json') | ConvertFrom-Json
$buildRoot = Join-Path $projectRoot ('.build/pinned-sdk-link-probe/run-' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $buildRoot
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'tmp') -Force | Out-Null
'UNVERIFIED CLIENT-IMPORT LINK DIAGNOSTIC. NOT A SERVER PACKAGE. DO NOT DEPLOY.' |
    Set-Content -LiteralPath (Join-Path $buildRoot 'NOT_A_BDS_PACKAGE.txt') -Encoding UTF8
$env:TEMP = Join-Path $buildRoot 'tmp'
$env:TMP = $env:TEMP
$dll = Join-Path $buildRoot 'BedrockInsight.dll'
$coreNames = @('admin_api','event','rollback','sqlite_api','storage')
$coreArguments = @($compile.compile_options)
foreach ($name in $coreNames) { $coreArguments += Join-Path $projectRoot "src/bedrock_insight/core/$name.cpp" }
$linkArguments = @('/nologo','/LD','/MD',"/Fe:$dll")
foreach ($object in $compile.objects) { $linkArguments += $object.path }
foreach ($name in $coreNames) { $linkArguments += Join-Path $buildRoot "$name.obj" }
$sqliteLibrary = Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'
$linkArguments += @('/link','/DEBUG',"/PDB:$(Join-Path $buildRoot 'BedrockInsight.pdb')",
    "/IMPLIB:$(Join-Path $buildRoot 'BedrockInsight.lib')",$referenceLibrary,$sqliteLibrary)
Push-Location -LiteralPath $buildRoot
try {
    & $compile.compiler @coreArguments *> (Join-Path $buildRoot 'core-compile.log')
    if ($LASTEXITCODE -ne 0) { throw "Core compile diagnostic failed: $LASTEXITCODE; see $buildRoot" }
    & $compile.compiler @linkArguments *> (Join-Path $buildRoot 'link.log')
    if ($LASTEXITCODE -ne 0) { throw "Native link diagnostic failed: $LASTEXITCODE; see $buildRoot" }
} finally { Pop-Location }
$pe = @(& $reader --file-headers --coff-exports --coff-imports $dll)
if ($LASTEXITCODE -ne 0 -or -not ($pe -match '^Format: COFF-x86-64$') -or
    -not ($pe -match '^\s*Name: ll_mod_load$') -or -not ($pe -match '^\s*Name: ll_memory_operator_overrided$')) {
    throw 'Native diagnostic must be x64 and export both actual SDK mod/allocator entries'
}
$pe | Set-Content -LiteralPath (Join-Path $buildRoot 'pe-inspection.txt') -Encoding UTF8
if ((Get-FileHash -LiteralPath $referenceLibrary -Algorithm SHA256).Hash -ine $expectedReferenceHash) {
    throw 'Reference library changed during diagnostic'
}
$profilePath = Join-Path $projectRoot 'packaging/bds-profile.json'
[ordered]@{
    compile_probe=$true; sdk_is_server=$false; sdk_version_matches_2660=$false
    matched_server_sdk_verified=$false; packaged=$false; launch_authorized=$false; bds_load_verified=$false
    warning='OLD CLIENT IMPORT METADATA: not a verified server SDK, release package or BDS loadable claim'
    header_source_commit=$compile.header_source_commit; header_catalog_sha256=$compile.header_catalog_sha256
    header_compile_evidence=(Join-Path $headerBuildRoot 'compile-evidence.json')
    native_objects=$compile.objects; header_dependencies=$compile.header_dependencies
    reference_import_library=$referenceLibrary; reference_import_sha256=$expectedReferenceHash
    import_metadata_inspection=$importMetadata
    compiler=$compile.compiler; core_arguments=$coreArguments; link_arguments=$linkArguments
    bds_profile_sha256=(Get-FileHash -LiteralPath $profilePath -Algorithm SHA256).Hash
    dll=$dll; dll_sha256=(Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $buildRoot 'build-evidence.json') -Encoding UTF8
'PINNED-HEADER NATIVE DLL LINK PASS: reference import metadata remains UNVERIFIED for server/BDS'
Get-FileHash -LiteralPath $dll -Algorithm SHA256 | Format-List
