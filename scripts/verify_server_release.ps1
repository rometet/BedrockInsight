$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
$profile = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../packaging/bds-profile.json') | ConvertFrom-Json
$manifest = [pscustomobject]@{name='LeviLamina';entry='LeviLamina.dll';version=$profile.levilamina_version;type='preload-native'}
$download = $profile.downloads[0]
Assert-BdsServerArchiveIdentity $download.size $download.sha256 $profile
Assert-BdsServerReleaseManifest $manifest $profile
# Text-only metadata fixtures, no fake SDK/binary/archive or launched process.
$header = @('Format: COFF-x86-64','  Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)','    IMAGE_FILE_DLL (0x2000)')
$server = $header + @('Export {','  Name: ?needed@@','  RVA: 0x1234','}')
$mod = $header + @('Export {','  Name: ll_mod_load','  RVA: 0x1111','}',
    'Export {','  Name: ll_memory_operator_overrided','  RVA: 0x2222','}',
    'Import {','  Name: LeviLamina.dll','  Symbol: ?needed@@ (1)','}')
$positive = Test-BdsServerImportResolution $server $mod
if (-not $positive.mod_ll_imports_resolved -or $positive.matched_server_sdk_verified -or
    $positive.launch_authorized -or $positive.bds_load_verified) { throw 'Static check must not assert SDK/runtime acceptance' }
$serverDependencies = $header + @('Import {','  Name: WS2_32.dll','  Symbol:  (18)','}',
    'DelayImport {','  Name: bedrock_runtime.dll','  Import {','    Symbol: x (0)','  }','}')
$dependencyInventory = Get-BdsPeDependencyModules $serverDependencies
if ($dependencyInventory.regular_import_modules[0] -cne 'WS2_32.dll' -or
    $dependencyInventory.delay_import_modules[0] -cne 'bedrock_runtime.dll' -or
    $dependencyInventory.dependencies_verified) { throw 'Server dependency inventory mismatch' }
$cases = @(
    { Assert-BdsServerArchiveIdentity ($download.size + 1) $download.sha256 $profile },
    { Assert-BdsServerArchiveIdentity $download.size ('0' * 64) $profile },
    { Assert-BdsServerArchiveIdentity $download.size '' $profile },
    { $bad=$manifest.psobject.Copy();$bad.version='26.51.9';Assert-BdsServerReleaseManifest $bad $profile },
    { $bad=$manifest.psobject.Copy();$bad.type='native';Assert-BdsServerReleaseManifest $bad $profile },
    { $bad=$manifest.psobject.Copy();$bad.entry='../LeviLamina.dll';Assert-BdsServerReleaseManifest $bad $profile },
    { Test-BdsServerImportResolution @($server | Where-Object {$_ -notmatch '^Format:'}) $mod },
    { Test-BdsServerImportResolution @($server -replace 'COFF-x86-64','COFF-i386') $mod },
    { Test-BdsServerImportResolution @($server | Where-Object {$_ -notmatch 'IMAGE_FILE_DLL'}) $mod },
    { Test-BdsServerImportResolution @($server -replace '0x1234','0x0') $mod },
    { Test-BdsServerImportResolution @($server -replace '\?needed@@','?other@@') $mod },
    { Test-BdsServerImportResolution $server @($mod -replace 'll_mod_load','wrong_entry') },
    { Test-BdsServerImportResolution $server @($mod -replace 'll_memory_operator_overrided','wrong_allocator') },
    { Test-BdsServerImportResolution $server @($mod -replace 'Symbol: \?needed@@','Symbol: ') },
    { Test-BdsServerImportResolution $server @($mod -replace 'LeviLamina.dll','Other.dll') },
    { Test-BdsServerImportResolution $server ($mod + @('Import {','  Name: sqlite3.dll','  Symbol: x (1)','}')) },
    { Test-BdsServerImportResolution $server ($mod + @('Import {','  Name: LeviLamina.dll','  Symbol: x (1)','}')) },
    { Test-BdsServerImportResolution ($server + @('Export {','  Name: ?needed@@','  RVA: 0x5678','}')) $mod },
    { Test-BdsServerImportResolution ($server + @('Export {')) $mod },
    { Test-BdsServerImportResolution $server ($mod + @('DelayImport {')) },
    { Get-BdsPeDependencyModules @($serverDependencies -replace 'WS2_32.dll','../outside.dll') },
    { Get-BdsPeDependencyModules @($serverDependencies | Where-Object { $_ -notmatch '^  Name:' }) },
    { Get-BdsPeDependencyModules ($serverDependencies + @('Import {','  Name: WS2_32.dll','}')) }
)
foreach ($case in $cases) {
    $rejected=$false
    try { & $case | Out-Null } catch { $rejected=$true }
    if (-not $rejected) { throw "Expected server release policy rejection: $case" }
}
"Server release static policy: PASS ($($cases.Count) text-only rejection cases; no DLL load)"
