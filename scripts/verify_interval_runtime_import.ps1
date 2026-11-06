# Parser tests only. No DLL loaded, SDK or BDS files written.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
$fixture=@'
DelayImport {
  Name: bedrock_runtime.dll
  Attributes: 0x1
  ModuleHandle: 0x100
  ImportAddressTable: 0x200
  ImportNameTable: 0x300
  BoundDelayImportTable: 0x0
  UnloadDelayImportTable: 0x0
  Import {
    Symbol: ?$getActivePlayerCount@Level@@QEBAHXZ (0)
    Address: 0x180100000
  }
}
'@
$fixture=$fixture.Replace("`r`n","`n")
$regular=@('Format: COFF-x86-64','Import {','  Name: LeviLamina.dll','  Symbol: example (0)','}')
$clean=@(Remove-BdsIntervalRuntimeDelayImport ($regular+@($fixture -split "`n")))
if (($clean -join "`n") -match 'DelayImport' -or ($clean -join "`n") -notmatch 'LeviLamina.dll') {throw 'Valid delay extraction failed'}
$busy=$fixture.Substring(0,$fixture.Length-1) + "  Import {`n    Symbol: ?`$tick@Level@@QEAAXXZ (0)`n    Address: 0x180100000`n  }`n}"
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($busy -split "`n"))
$drop=$busy.Substring(0,$busy.Length-1) + "  Import {`n    Symbol: ?`$drop@Player@@QEAA_NAEBVItemStack@@_N@Z (0)`n    Address: 0x180200000`n  }`n}"
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($drop -split "`n"))
$death=$drop.Substring(0,$drop.Length-1) + "  Import {`n    Symbol: ?`$die@Player@@QEAAXAEBVActorDamageSource@@@Z (0)`n    Address: 0x180300000`n  }`n  Import {`n    Symbol: ??0StackResultStorageEntity@@QEAA@AEBVWeakStorageEntity@@@Z (0)`n    Address: 0x180400000`n  }`n}"
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($death -split "`n"))
$pickup=$death.Substring(0,$death.Length-1) + "  Import {`n    Symbol: ?take@Player@@QEAA_NAEAVActor@@HH@Z (0)`n    Address: 0x180500000`n  }`n}"
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($pickup -split "`n"))
$pickupWithoutWeak=$drop.Substring(0,$drop.Length-1) + "  Import {`n    Symbol: ?take@Player@@QEAA_NAEAVActor@@HH@Z (0)`n    Address: 0x180500000`n  }`n}"
$jumpTail="  Import {`n    Symbol: ?jumpFromGround@Mob@@QEAAXXZ (0)`n    Address: 0x180600000`n  }`n  Import {`n    Symbol: ?getVehicle@Actor@@QEBAPEAV1@XZ (0)`n    Address: 0x180700000`n  }`n}"
$jump=$pickup.Substring(0,$pickup.Length-1)+$jumpTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($jump -split "`n"))
$jumpWithoutWeak=$drop.Substring(0,$drop.Length-1)+$jumpTail
$jumpWithoutVehicle=$pickup.Substring(0,$pickup.Length-1)+"  Import {`n    Symbol: ?jumpFromGround@Mob@@QEAAXXZ (0)`n    Address: 0x180600000`n  }`n}"
$killTail="  Import {`n    Symbol: ?`$die@Mob@@QEAAXAEBVActorDamageSource@@@Z (0)`n    Address: 0x180800000`n  }`n}"
$kill=$jump.Substring(0,$jump.Length-1)+$killTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($kill -split "`n"))
$killWithoutWeak=$drop.Substring(0,$drop.Length-1)+$killTail
$useTail="  Import {`n    Symbol: ?`$useItem@GameMode@@QEAA_NAEAVItemStack@@W4HandSlot@@@Z (0)`n    Address: 0x180900000`n  }`n}"
$use=$kill.Substring(0,$kill.Length-1)+$useTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($use -split "`n"))
$useWithoutWeak=$drop.Substring(0,$drop.Length-1)+$useTail
$hurtTail="  Import {`n    Symbol: ?hurtAndBreak@ItemStackBase@@QEAA_NHPEAVActor@@@Z (0)`n    Address: 0x180A00000`n  }`n}"
$hurt=$use.Substring(0,$use.Length-1)+$hurtTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($hurt -split "`n"))
$hurtWithoutWeak=$drop.Substring(0,$drop.Length-1)+$hurtTail
$damageTail="  Import {`n    Symbol: ?getDamageValue@ItemStackBase@@QEBAFXZ (0)`n    Address: 0x180B00000`n  }`n}"
$damage=$hurt.Substring(0,$hurt.Length-1)+$damageTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($damage -split "`n"))
$damageWithoutHurt=$use.Substring(0,$use.Length-1)+$damageTail
$tradeTail="  Import {`n    Symbol: ?`$openTrading@ServerPlayer@@QEAAXAEBUActorUniqueID@@_N@Z (0)`n    Address: 0x180C00000`n  }`n}"
$trade=$damage.Substring(0,$damage.Length-1)+$tradeTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($trade -split "`n"))
$breedTail='  Import {'+"`n"+'    Symbol: ?mate@BreedableComponent@@QEAA?AV?$vector@VWeakEntityRef@@V?$allocator@VWeakEntityRef@@@std@@@std@@AEAVActor@@0@Z (0)'+"`n    Address: 0x180D00000`n  }`n}"
$breed=$trade.Substring(0,$trade.Length-1)+$breedTail
$null=Remove-BdsIntervalRuntimeDelayImport ($regular+@($breed -split "`n"))
$breedWithoutWeak=$drop.Substring(0,$drop.Length-1)+$breedTail
$count=0
foreach($text in @(
    $fixture.Replace('bedrock_runtime.dll','evil.dll'),
    $fixture.Replace('getActivePlayerCount','guessedFunction'),
    $drop.Replace('_N@Z','H@Z'),
    $drop.Replace('?$drop@Player@@QEAA_NAEBVItemStack@@_N@Z','?$tick@Level@@QEAAXXZ'),
    $death.Replace('ActorDamageSource','GuessedDamageSource'),
    $death.Replace('WeakStorageEntity','GuessedWeakStorage'),
    $pickup.Replace('Actor@@HH@Z','Actor@@H@Z'),
    $pickupWithoutWeak,
    $jump.Replace('jumpFromGround@Mob','jumpFromGround@Player'),
    $jump.Replace('?getVehicle@Actor@@QEBAPEAV1@XZ','?getVehicle@Actor@@QEBAPEBV1@XZ'),
    $jumpWithoutVehicle,
    $jumpWithoutWeak,
    $kill.Replace('?$die@Mob@@','?$die@Actor@@'),
    $killWithoutWeak,
    $useWithoutWeak,
    $hurtWithoutWeak,
    $damageWithoutHurt,
    $damage.Replace('getDamageValue@ItemStackBase@@QEBAFXZ','getDamageValue@ItemStackBase@@QEBAHXZ'),
    $damage.Replace('getDamageValue@ItemStackBase@@QEBAFXZ','getDamageValue@ItemStackBase@@QEAAFXZ'),
    $trade.Replace('openTrading@ServerPlayer','openTrading@Player'),
    $trade.Replace('QEAAXAEBUActorUniqueID','QEAA_NAEBUActorUniqueID'),
    $trade.Replace('AEBUActorUniqueID@@_N@Z','AEBUActorUniqueID@@@Z'),
    $breed.Replace('mate@BreedableComponent','mate@Actor'),
    $breed.Replace('std@@AEAVActor@@0@Z','std@@AEBVActor@@0@Z'),
    $breedWithoutWeak,
    $hurt.Replace('hurtAndBreak@ItemStackBase','hurtAndBreak@ItemStack'),
    $hurt.Replace('QEAA_NHPEAVActor','QEAAXHPEAVActor'),
    $hurt.Replace('QEAA_NHPEAVActor','QEAA_NHPEBVActor'),
    $use.Replace('?$useItem@GameMode@@','?$useItem@Player@@'),
    $use.Replace('AEAVItemStack@@W4HandSlot','AEAVItemStackBase@@W4HandSlot'),
    $use.Replace('QEAA_NAEAVItemStack@@W4HandSlot','QEAAXAEAVItemStack@@W4HandSlot'),
    $drop.Replace('?$drop@Player@@QEAA_NAEBVItemStack@@_N@Z','?$die@Player@@QEAAXAEBVActorDamageSource@@@Z'),
    $fixture.Replace('(0)','(1)'),
    $fixture.Replace('Attributes: 0x1','Attributes: 0x0'),
    $fixture.Replace('BoundDelayImportTable: 0x0','BoundDelayImportTable: 0x10'),
    $fixture.Replace('UnloadDelayImportTable: 0x0','UnloadDelayImportTable: 0x10'),
    $fixture.Replace('    Address:',"    Symbol: second (0)`n    Address:"),
    ($fixture+"`n"+$fixture),
    $fixture.Substring(0,$fixture.Length-1),
    ''
)) {
    $failed=$false
    try { $null=Remove-BdsIntervalRuntimeDelayImport (@($text -split "`n")) } catch {$failed=$true}
    if(-not $failed){throw 'Malformed or unreviewed delay input accepted'}
    ++$count
}
"Count/tick/action/damage-value/trade/breed MCAPI delay whitelist parser PASS (12 valid, $count rejected); no load/runtime proof"
