# Static PE/manifest policy only. Never loads a DLL or authorizes BDS execution.
function Assert-BdsServerArchiveIdentity($Length, [string]$Sha256, $Profile) {
    $expected = @($Profile.downloads | Where-Object {
        $_.name -ceq "levilamina-v$($Profile.levilamina_version)-server-release-windows-x64.zip"
    })
    if ($expected.Count -ne 1 -or $Length -ne $expected[0].size -or
        $Sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $Sha256 -ine $expected[0].sha256) {
        throw 'Official server archive size/hash mismatch'
    }
}

function Assert-BdsServerReleaseManifest($Manifest, $Profile) {
    if ($Manifest.name -cne 'LeviLamina' -or $Manifest.entry -cne 'LeviLamina.dll' -or
        $Manifest.version -cne $Profile.levilamina_version -or $Manifest.type -cne 'preload-native') {
        throw 'Official server release manifest mismatch'
    }
}

function Assert-BdsPeX64Dll([string[]]$Inspection) {
    if (@($Inspection | Where-Object { $_ -match '^Format:' }).Count -ne 1 -or
        -not ($Inspection -ccontains 'Format: COFF-x86-64') -or
        -not ($Inspection -match '^  Machine: IMAGE_FILE_MACHINE_AMD64 ') -or
        -not ($Inspection -match '^    IMAGE_FILE_DLL ')) {
        throw 'Expected one x64 PE DLL inspection'
    }
}

function Get-BdsPeNamedExports([string[]]$Inspection) {
    Assert-BdsPeX64Dll $Inspection
    $names = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $inside = $false; $name = $null; $rva = $null
    foreach ($line in $Inspection) {
        if ($line -ceq 'Export {') {
            if ($inside) { throw 'Malformed PE export record' }
            $inside = $true; $name = $null; $rva = $null
        } elseif ($inside -and $line -match '^  Name: (.*)$') {
            if ($null -ne $name) { throw 'Duplicate PE export name field' }
            $name = $Matches[1]
        } elseif ($inside -and $line -match '^  RVA: (0x[0-9A-Fa-f]+)$') {
            if ($null -ne $rva) { throw 'Duplicate PE export RVA field' }
            $rva = $Matches[1]
        } elseif ($inside -and $line -ceq '}') {
            if (-not $name -or $name -match '\s' -or -not $rva -or
                [Convert]::ToUInt64($rva.Substring(2),16) -eq 0 -or -not $names.Add($name)) {
                throw 'Missing, duplicate, ordinal-only or zero-RVA PE export'
            }
            $inside = $false
        }
    }
    if ($inside -or $names.Count -eq 0) { throw 'Incomplete or empty PE exports' }
    return @($names)
}

function Get-BdsPeNamedImports([string[]]$Inspection) {
    Assert-BdsPeX64Dll $Inspection
    $modules = @{}; $inside = $false; $module = $null
    $symbols = [Collections.Generic.List[string]]::new()
    foreach ($line in $Inspection) {
        if ($line -match '^DelayImport') { throw 'Delay imports are not supported by this static policy' }
        if ($line -ceq 'Import {') {
            if ($inside) { throw 'Malformed PE import record' }
            $inside = $true; $module = $null; $symbols.Clear()
        } elseif ($inside -and $line -match '^  Name: (.*)$') {
            if ($null -ne $module) { throw 'Duplicate PE import module field' }
            $module = $Matches[1]
        } elseif ($inside -and $line -match '^  Symbol:') {
            if ($line -notmatch '^  Symbol: (\S+) \([0-9]+\)$') { throw 'Ordinal-only or malformed PE import' }
            $symbols.Add($Matches[1])
        } elseif ($inside -and $line -ceq '}') {
            if (-not $module -or $module -notmatch '^[a-zA-Z0-9_.-]+\.dll$' -or
                $modules.ContainsKey($module) -or $symbols.Count -eq 0) {
                throw 'Missing, duplicate or empty PE import module'
            }
            $modules[$module] = $symbols.ToArray()
            $inside = $false
        }
    }
    if ($inside -or $modules.Count -eq 0) { throw 'Incomplete or empty PE imports' }
    return $modules
}

function Get-BdsPeDependencyModules([string[]]$Inspection) {
    # Module-name inventory only, not symbol resolution. The official loader
    # legitimately has Winsock ordinals and nested bedrock_runtime delay imports.
    Assert-BdsPeX64Dll $Inspection
    $regular = [Collections.Generic.List[string]]::new()
    $delay = [Collections.Generic.List[string]]::new()
    $section = $null
    foreach ($line in $Inspection) {
        if ($line -cin @('Import {','DelayImport {')) {
            if ($null -ne $section) { throw 'Missing PE dependency module name' }
            $section = $line
        } elseif ($null -ne $section -and $line -match '^  Name: (.*)$') {
            $module = $Matches[1]
            if ($module -notmatch '^[a-zA-Z0-9_.-]+\.dll$') { throw 'Invalid PE dependency module name' }
            $target = $regular
            if ($section -ceq 'DelayImport {') { $target = $delay }
            if (@($target) -icontains $module) { throw 'Duplicate PE dependency module name' }
            $target.Add($module)
            $section = $null
        } elseif ($null -ne $section -and $line -ceq '}') { throw 'Missing PE dependency module name' }
    }
    if ($null -ne $section -or $regular.Count + $delay.Count -eq 0) { throw 'Incomplete or empty PE dependency inventory' }
    return [pscustomobject]@{regular_import_modules=$regular.ToArray();delay_import_modules=$delay.ToArray();dependencies_verified=$false}
}

function Remove-BdsIntervalRuntimeDelayImport([string[]]$Inspection) {
    # Historical function name retained for callers. Exact count/tick/drop
    # MCAPIs only; action/death names verified in pinned 26.60 compiled objects.
    # Do not generally permit delay imports, ordinals or arbitrary engine APIs.
    $text=$Inspection -join "`n"
    if (@($Inspection | Where-Object {$_ -match '^DelayImport'}).Count -ne 1) {
        throw 'Interval candidate requires exactly one runtime delay module'
    }
    $pattern='(?m)^DelayImport \{\n  Name: bedrock_runtime\.dll\n  Attributes: 0x1\n' +
        '  ModuleHandle: 0x[0-9A-Fa-f]+\n  ImportAddressTable: 0x[0-9A-Fa-f]+\n' +
        '  ImportNameTable: 0x[0-9A-Fa-f]+\n  BoundDelayImportTable: 0x0\n' +
        '  UnloadDelayImportTable: 0x0\n' +
        '(?:  Import \{\n    Symbol: (?<api>\S+) \(0\)\n' +
        '    Address: 0x[0-9A-Fa-f]+\n  \}\n){1,48}\}$'
    $match=[regex]::Match($text,$pattern)
    if (-not $match.Success) { throw 'Unreviewed interval runtime delay import shape/symbol' }
    $allowed=@('?$getActivePlayerCount@Level@@QEBAHXZ','?$tick@Level@@QEAAXXZ',
        '?$drop@Player@@QEAA_NAEBVItemStack@@_N@Z',
        '?$die@Player@@QEAAXAEBVActorDamageSource@@@Z',
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z',
        '?take@Player@@QEAA_NAEAVActor@@HH@Z',
        '?jumpFromGround@Mob@@QEAAXXZ','?getVehicle@Actor@@QEBAPEAV1@XZ',
        '?$die@Mob@@QEAAXAEBVActorDamageSource@@@Z',
        '?$useItem@GameMode@@QEAA_NAEAVItemStack@@W4HandSlot@@@Z',
        '?hurtAndBreak@ItemStackBase@@QEAA_NHPEAVActor@@@Z',
        '?getDamageValue@ItemStackBase@@QEBAFXZ',
        '?$destroyBlock@GameMode@@QEAA_NAEBVBlockPos@@E@Z',
        '?$buildBlock@GameMode@@QEAA_NAEBVBlockPos@@EW4HandSlot@@_N@Z',
        '?getDimensionBlockSource@Actor@@QEBAAEAVBlockSource@@XZ',
        '?isFlying@Player@@QEBA_NXZ',
        '?isInWater@Actor@@QEBA_NXZ',
        '?postRequest@CraftHandlerBase@@QEAAX_N@Z',
        # Actual compiler/PE names; pinned26.60 MCAPI declarations reviewed.
        # This does NOT establish successful trading or offspring attribution.
        '?$openTrading@ServerPlayer@@QEAAXAEBUActorUniqueID@@_N@Z',
        '?mate@BreedableComponent@@QEAA?AV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEAVActor@@0@Z',
        '??$parse@H@CommandRegistry@@QEBA_NPEAXAEBUParseToken@0@AEBVCommandOrigin@@HAEAV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEAV?$vector@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$allocator@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@@4@@Z',
        '??$parse@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@CommandRegistry@@QEBA_NPEAXAEBUParseToken@0@AEBVCommandOrigin@@HAEAV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@AEAV?$vector@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V?$allocator@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@@4@@Z',
        '??$typeid_storage_impl@VCommandRegistry@@H@Bedrock@@YA?AVtype_id_ref@ll@@XZ',
        '??$typeid_storage_impl@VCommandRegistry@@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Bedrock@@YA?AVtype_id_ref@ll@@XZ',
        '??1CommandOutputParameter@@QEAA@XZ',
        '?addMessage@CommandOutput@@QEAAXV?$basic_string_view@DU?$char_traits@D@std@@@std@@AEBV?$vector@VCommandOutputParameter@@V?$allocator@VCommandOutputParameter@@@std@@@3@W4CommandOutputMessageType@@@Z',
        '?collectOptionalArguments@Command@@UEAA_NXZ',
        '?success@CommandOutput@@QEAAXV?$basic_string_view@DU?$char_traits@D@std@@@std@@AEBV?$vector@VCommandOutputParameter@@V?$allocator@VCommandOutputParameter@@@std@@@3@@Z',
        '?$openTrading@ServerPlayer@@QEAAXAEBUActorUniqueID@@_N@Z',
        '?mate@BreedableComponent@@QEAA?AV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEAVActor@@0@Z',
        '?$onProjectileHit@TargetBlock@@QEBAXAEAVBlockSource@@AEBVBlockPos@@AEBVActor@@@Z',
        '?getOwner@Actor@@QEBAPEAVMob@@XZ',
        # Confirmed post-outcome block interaction (InteractionResult.mSuccess).
        '?$useItemOn@GameMode@@QEAA?AVInteractionResult@@AEAVItemStack@@AEBVBlockPos@@EAEBVVec3@@W4HandSlot@@PEBVBlock@@_N@Z',
        # Committed trade result transfer (ItemStackNetResult::Success).
        '?_handleTransfer@ItemStackRequestActionHandler@@QEAA?AW4ItemStackNetResult@@AEBVItemStackRequestActionTransferBase@@_N11@Z',
        # Surface vs underwater movement split (Water material).
        '?isUnderLiquid@Actor@@QEBA_NW4MaterialType@v1_26_20@SharedTypes@@@Z')
    $names=@($match.Groups['api'].Captures | ForEach-Object {$_.Value})
    if (@($names | Where-Object {$_ -cnotin $allowed}).Count -or
        @($names | Select-Object -Unique).Count -ne $names.Count -or
        '?$getActivePlayerCount@Level@@QEBAHXZ' -cnotin $names) {
        throw 'Unreviewed or duplicate runtime delay symbol'
    }
    if (('?$die@Player@@QEAAXAEBVActorDamageSource@@@Z' -cin $names) -ne
        ('??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cin $names)) {
        throw 'Incomplete reviewed death/weak-reference API pair'
    }
    if ('?take@Player@@QEAA_NAEAVActor@@HH@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Pickup inventory observation requires reviewed weak-reference API'
    }
    if (('?jumpFromGround@Mob@@QEAAXXZ' -cin $names) -ne
        ('?getVehicle@Actor@@QEBAPEAV1@XZ' -cin $names)) {
        throw 'Incomplete reviewed jump/vehicle API pair'
    }
    if ('?jumpFromGround@Mob@@QEAAXXZ' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Jump observation requires reviewed weak-reference API'
    }
    if ('?$die@Mob@@QEAAXAEBVActorDamageSource@@@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Mob death observation requires reviewed weak-reference API'
    }
    if ('?$useItem@GameMode@@QEAA_NAEAVItemStack@@W4HandSlot@@@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Use result observation requires reviewed weak-reference API'
    }
    if ('?$useItemOn@GameMode@@QEAA?AVInteractionResult@@AEAVItemStack@@AEBVBlockPos@@EAEBVVec3@@W4HandSlot@@PEBVBlock@@_N@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Confirmed interaction observation requires reviewed weak-reference API'
    }
    if ('?hurtAndBreak@ItemStackBase@@QEAA_NHPEAVActor@@@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Hurt slot observation requires reviewed weak-reference API'
    }
    if ('?mate@BreedableComponent@@QEAA?AV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEAVActor@@0@Z' -cin $names -and
        '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z' -cnotin $names) {
        throw 'Breed offspring observation requires reviewed weak-reference API'
    }
    # Actual llvm-readobj/nm symbol, verified MCAPI short getDamageValue() const
    # in the pinned26.60 ItemStackBase declaration. Not a new arbitrary API gate.
    if ('?getDamageValue@ItemStackBase@@QEBAFXZ' -cin $names) {
        foreach($required in @('?hurtAndBreak@ItemStackBase@@QEAA_NHPEAVActor@@@Z',
            '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z')) {
            if($required -cnotin $names){throw 'Damage-value observation requires the reviewed hurt/weak API set'}
        }
    }
    $minedBlockApi='?$destroyBlock@GameMode@@QEAA_NAEBVBlockPos@@E@Z'
    $placedBlockApi='?$buildBlock@GameMode@@QEAA_NAEBVBlockPos@@EW4HandSlot@@_N@Z'
    if (($minedBlockApi -cin $names) -or ($placedBlockApi -cin $names)) {
        foreach ($required in @($minedBlockApi,$placedBlockApi,
            '?getDimensionBlockSource@Actor@@QEBAAEAVBlockSource@@XZ',
            '??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z')) {
            if ($required -cnotin $names) { throw 'Block state observation requires the reviewed GameMode/BlockSource/weak API set' }
        }
    }
    return ($text.Remove($match.Index,$match.Length) -split "`n")
}

function Test-BdsServerImportResolution([string[]]$ServerInspection, [string[]]$ModInspection,
        [switch]$AllowIntervalRuntimeApi) {
    $exports = @(Get-BdsPeNamedExports $ServerInspection)
    $modExports = @(Get-BdsPeNamedExports $ModInspection)
    foreach ($required in @('ll_mod_load','ll_memory_operator_overrided')) {
        if ($modExports -cnotcontains $required) { throw "Missing native mod export: $required" }
    }
    $regularInspection=$ModInspection
    if ($AllowIntervalRuntimeApi) { $regularInspection=Remove-BdsIntervalRuntimeDelayImport $ModInspection }
    $imports = Get-BdsPeNamedImports $regularInspection
    if (-not $imports.ContainsKey('LeviLamina.dll')) { throw 'Missing named LeviLamina imports' }
    foreach ($module in $imports.Keys) {
        if ($module -ine 'LeviLamina.dll' -and $module -notmatch
            '^(KERNEL32|MSVCP140|VCRUNTIME140(?:_1)?)\.dll$|^api-ms-win-crt-[a-z0-9-]+\.dll$') {
            throw "Unreviewed native mod import module: $module"
        }
    }
    $missing = @($imports['LeviLamina.dll'] | Where-Object { $_ -cnotin $exports })
    if ($missing.Count) { throw "Unresolved LeviLamina export(s): $($missing -join ', ')" }
    return [pscustomobject]@{
        server_export_count=$exports.Count
        mod_ll_import_count=@($imports['LeviLamina.dll']).Count
        mod_ll_import_names=@($imports['LeviLamina.dll'])
        mod_import_modules=@($imports.Keys | Sort-Object)
        mod_ll_imports_resolved=$true
        interval_runtime_api_import_whitelisted=$AllowIntervalRuntimeApi.IsPresent
        matched_server_sdk_verified=$false
        launch_authorized=$false
        bds_load_verified=$false
        limits='Named-import resolution only; not complete SDK, object ABI, allocator/hook execution, runtime dependency or BDS behavior proof.'
    }
}
