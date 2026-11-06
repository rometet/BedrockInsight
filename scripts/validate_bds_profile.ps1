# Validate recorded build inputs, not runtime compatibility. This never starts
# a process, resolves packages, downloads files, or evaluates SDK Lua code.
function Assert-BdsNativeCaptureManifest($NativeManifest, $Profile) {
    # LL's preload-native manager is excluded from the ordinary dependency
    # graph by ModRegistrar::loadManifest. Runtime checks its exported version.
    if ($NativeManifest.name -cne 'BedrockInsight' -or $NativeManifest.platform -cne 'server' -or
        $NativeManifest.type -cne 'native' -or $NativeManifest.entry -cne 'BedrockInsight.dll' -or
        @($NativeManifest.dependencies).Count -ne 0 -or
        $NativeManifest.extraInfo.requiredLeviLamina -cne $Profile.levilamina_version) {
        throw 'Native manifest does not match the pinned server profile'
    }
}
function Assert-BdsProfileInputs {
    param([Parameter(Mandatory)]$SdkInputs,
          [Parameter(Mandatory)]$Profile,
          [Parameter(Mandatory)]$NativeManifest,
          [Parameter(Mandatory)]$Tooth)
    if ($Profile.schema_version -ne 1) { throw 'Unsupported BDS profile schema' }
    foreach ($pin in $Profile.sdk_dependencies.PSObject.Properties) {
        $selected = @($SdkInputs.dependencies | Where-Object { $_.name -ceq $pin.Name })
        if ($selected.Count -ne 1 -or $selected[0].version -cne $pin.Value) {
            throw "SDK dependency mismatch: $($pin.Name) requires exactly $($pin.Value) for LL $($Profile.levilamina_version)"
        }
    }
    foreach ($define in $Profile.public_defines) {
        if ($SdkInputs.defines -cnotcontains $define) { throw "SDK ABI definition mismatch: requires $define" }
    }
    Assert-BdsNativeCaptureManifest $NativeManifest $Profile
    if (@($Tooth.variants).Count -ne 1) { throw 'Expected exactly one server Tooth variant' }
    $variant = $Tooth.variants[0]
    $placements = @($variant.assets | ForEach-Object { $_.placements })
    if ($variant.platform -cne 'win-x64' -or
        $variant.dependencies.'github.com/LiteLDev/LeviLamina' -cne $Profile.levilamina_version -or
        $placements.Count -ne 1 -or $placements[0].src -cne 'BedrockInsight/' -or
        $placements[0].dest -cne $Profile.mod_destination -or $placements[0].type -cne 'dir') {
        throw 'Tooth manifest does not match the pinned server profile placement/dependency'
    }
}
