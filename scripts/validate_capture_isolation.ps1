. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
function Assert-BdsCaptureLoaderConfig($Config, [ValidateSet(41,42)][int]$ExpectedVersion=41) {
    if (($Config.version -isnot [int] -and $Config.version -isnot [long]) -or
        $Config.version -ne $ExpectedVersion -or $Config.language -cne 'en') { throw "Expected isolated loader config integer v$ExpectedVersion" }
    foreach ($value in @($Config.modules.crashLogger.enabled,$Config.modules.crashLogger.uploadToSentry,
        $Config.targeted.enableStatistics,$Config.targeted.checkRunningBDS,$Config.targeted.forceEnableCheatCommands,
        $Config.targeted.preserveInputLine,$Config.targeted.netherNetPatch.enable)) {
        if ($value -isnot [bool] -or $value) { throw 'Unsafe isolated loader configuration' }
    }
}
function Assert-BdsCaptureBuild($Build, $Profile, [string]$ProfileHash, [string]$DllHash) {
    foreach ($name in @('official_server_functions_verified','prelink_completed')) {
        if ($Build.$name -isnot [bool] -or -not $Build.$name) { throw "Missing capture build verification: $name" }
    }
    foreach ($name in @('old_client_import_library_used','normal_release_evidence','matched_server_sdk_verified','bds_load_verified')) {
        if ($Build.$name -isnot [bool] -or $Build.$name) { throw "Not an unverified server-derived candidate: $name" }
    }
    # A native MCAPI call must not reach the regular MSVC delay-load helper.
    # The C8F... trial loaded/enabled but crashed C06D007E at the first sample.
    if ($Build.msvc_delayimp_used -isnot [bool] -or $Build.msvc_delayimp_used -or
        $Build.symbol_provider.source_commit -cne '6c93ec45c8455992ee726d92df60316c8e731c44' -or
        $Build.symbol_provider.sha256 -ine 'B569E4BB25EE66FB9439D1D4B42C2EDCEF41878222ED9684B4118D986301ABFB' -or
        $Build.resolution.mod_ll_import_names -cnotcontains '?resolveSymbol@memory@ll@@YAPEAXPEBD@Z') {
        throw 'Capture candidate lacks the reviewed LL26.60 SymbolProvider delay resolver'
    }
    if ($Build.artifact_kind -cne 'official-server-function-import-candidate' -or
        $Build.bds_profile_sha256 -ine $ProfileHash -or
        $Build.header_source_commit -cne $Profile.levilamina_commit -or
        $Build.header_catalog_sha256 -ine $Profile.header_catalog_sha256 -or
        $Build.dll_sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $DllHash -ine $Build.dll_sha256 -or
        $Build.resolution.mod_ll_imports_resolved -isnot [bool] -or -not $Build.resolution.mod_ll_imports_resolved) {
        throw 'Capture candidate identity/profile/header/import mismatch'
    }
}
