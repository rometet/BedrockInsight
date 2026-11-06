# Compile one declaration/import probe with already-verified cached SDK options.
# No SDK download/rebuild, prelink, mod DLL, BDS process or candidate gate change.
param([Parameter(Mandatory)][string]$HeaderEvidence)
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$evidencePath=[IO.Path]::GetFullPath($HeaderEvidence)
if(-not $evidencePath.StartsWith((Join-Path $project '.build')+'\',[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Use existing project-owned header compile evidence only'
}
$evidence=Get-Content -Raw -LiteralPath $evidencePath|ConvertFrom-Json
if($evidence.header_source_commit -cne '072b9ffe0e9d9e0ec846f11d294880e892fe7770' -or
   $evidence.header_catalog_sha256 -cne '1AE4DD1E4F54F7DA1C9D8A88E29F0897109F052F91F2EF12D2C1FF9DA76C5823' -or
   $evidence.compile_only -isnot [bool] -or -not $evidence.compile_only) {
    throw 'Need the pinned26.60 compile-only evidence; no guessed SDK'
}
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$nm=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-nm.exe'
$root=Join-Path $project ('.build/craft-api-probe/run-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'tmp') -Force|Out-Null
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$source=Join-Path $project 'tests/craft_api_compile_probe.cpp'
$object=Join-Path $root 'craft-api-probe.obj'
$arguments=@($evidence.compile_options)+@($source,"/Fo:$object")
Invoke-BdsLoggedNative $compiler $arguments (Join-Path $root 'compile.log')
Invoke-BdsLoggedNative $nm @('--undefined-only',$object) (Join-Path $root 'undefined-symbols.log')
[ordered]@{compile_only=$true;bds_loaded=$false;dll_linked=$false;
    header_evidence_sha256=(Get-FileHash -LiteralPath $evidencePath).Hash;
    source_sha256=(Get-FileHash -LiteralPath $source).Hash;object_sha256=(Get-FileHash -LiteralPath $object).Hash;
    limits='Exact official declarations/import names, NOT caller ordering, completed craft, runtime ABI or capture proof'}|
    ConvertTo-Json|Set-Content -LiteralPath (Join-Path $root 'probe-evidence.json') -Encoding UTF8
"Craft API declaration probe: $root (objects only; not craft acceptance)"
