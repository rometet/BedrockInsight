# Static inspection of freshly staged official runtime inputs. No DLL loading.
param([switch]$PassThru)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
. (Join-Path $PSScriptRoot 'server_function_exports.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
if (-not $env:BEDROCKINSIGHT_CLANGCL_ROOT) { throw 'BEDROCKINSIGHT_CLANGCL_ROOT is required' }
$reader=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
if (-not [IO.File]::Exists($reader)) { throw 'Missing LLVM metadata reader' }
$stage=& (Join-Path $PSScriptRoot 'stage_official_inputs.ps1') -PassThru
$evidence=Get-Content -Raw -LiteralPath $stage.evidence_path | ConvertFrom-Json
$inspections=@{}
foreach ($id in @('levilamina','preloader','prelink','peeditor','crashlogger')) {
    $archive=@($evidence.archives | Where-Object { $_.id -ceq $id })[0]
    $binary=@($archive.selected_files | Where-Object { $_.entry -match '\.(dll|exe)$' })[0]
    $log=Join-Path $stage.root ($id + '-pe.txt')
    Invoke-BdsLoggedNative $reader @('--file-headers','--sections','--coff-exports','--coff-imports',$binary.path) $log
    $pe=@(Get-Content -LiteralPath $log)
    if (@($pe | Where-Object { $_ -match '^Format:' }).Count -ne 1 -or
        -not ($pe -ccontains 'Format: COFF-x86-64') -or -not ($pe -match '^  Machine: IMAGE_FILE_MACHINE_AMD64 ')) {
        throw "Expected one AMD64 PE: $id"
    }
    $inspections[$id]=$pe
}
$preloaderNames=@(Get-BdsPeNamedExports $inspections.preloader)
$llPreloaderImports=@(Get-BdsRegularModuleImports $inspections.levilamina 'PreLoader.dll')
$missing=@($llPreloaderImports | Where-Object { $_ -cnotin $preloaderNames })
if ($missing.Count) { throw "Unresolved PreLoader names: $($missing -join ', ')" }
$preloaderClass=Get-BdsServerFunctionExports $inspections.preloader
$preloaderDependencies=Get-BdsPeDependencyModules $inspections.preloader
if ($preloaderDependencies.delay_import_modules.Count -ne 0) { throw 'Unreviewed PreLoader delay dependency' }
foreach ($name in $preloaderDependencies.regular_import_modules) {
    if ($name -notmatch '^(KERNEL32|MSVCP140|VCRUNTIME140(?:_1)?)\.dll$|^api-ms-win-crt-[a-z0-9-]+\.dll$') {
        throw "Unreviewed PreLoader dependency: $name"
    }
}
# Ensure inspected payloads still match the fresh extraction evidence.
foreach ($archive in $evidence.archives) {
    foreach ($file in $archive.selected_files) {
        if ((Get-FileHash -LiteralPath $file.path -Algorithm SHA256).Hash -ine $file.sha256) { throw 'Staged official payload changed' }
    }
}
$path=Join-Path $stage.root 'runtime-inspection-evidence.json'
[pscustomobject]@{inspected_utc=[DateTime]::UtcNow.ToString('o');staging_evidence=$stage.evidence_path;
    staging_evidence_sha256=(Get-FileHash -LiteralPath $stage.evidence_path -Algorithm SHA256).Hash;
    ll_preloader_import_names=$llPreloaderImports;ll_preloader_named_imports_resolved=$true;
    preloader_export_count=$preloaderNames.Count;preloader_function_count=$preloaderClass.functions.Count;
    preloader_nonfunction_exports=$preloaderClass.excluded;preloader_dependency_modules=$preloaderDependencies;
    ll_dependency_modules=(Get-BdsPeDependencyModules $inspections.levilamina);
    runtime_dependencies_verified=$false;matched_server_sdk_verified=$false;
    launch_authorized=$false;bds_load_verified=$false;
    limits='Only staged PE architecture and named LL-to-PreLoader export resolution. Data/function ABI, Windows CRT availability, bedrock_runtime generation and BDS execution remain unverified.'
} | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
if ($PassThru) { [pscustomobject]@{root=$stage.root;evidence_path=$path;staging_evidence=$stage.evidence_path} }
else { "Official runtime static inspection PASS ($($llPreloaderImports.Count) PreLoader names); no DLL load. Evidence: $path" }
