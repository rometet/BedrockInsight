$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_official_inputs.ps1')
$pins=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../packaging/official-inputs.json') | ConvertFrom-Json
$profile=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot '../packaging/bds-profile.json') | ConvertFrom-Json
Assert-BdsInputPins $pins $profile
Assert-BdsInputEntry 'include/pl/Hook.h' 2796 0
Assert-BdsInputEntry 'include/' 0 16
Assert-BdsInputEntry 'definitions/persona/02_ Noor_Dcast.json' 1000 0
Assert-BdsInputEntry 'bedrock_server.exe' 232647024 0 268435456
Assert-BdsPrelinkOutput 'bedrock_runtime_api.lib' 1392
$snapshot=Read-BdsJsonSnapshot (Join-Path $PSScriptRoot '../packaging/bds-profile.json')
Assert-BdsJsonSnapshotUnchanged $snapshot
$imports=@('Import {','  Name: WS2_32.dll','  Symbol:  (18)','}',
    'Import {','  Name: PreLoader.dll','  Symbol: ?needed@@ (1)','}',
    'DelayImport {','  Name: bedrock_runtime.dll','  Import {','    Symbol: x (0)','  }','}')
$result=@(Get-BdsRegularModuleImports $imports 'PreLoader.dll')
if ($result.Count -ne 1 -or $result[0] -cne '?needed@@') { throw 'Runtime import extraction mismatch' }
$cases=@(
    { Assert-BdsInputEntry '../escape.dll' 1 0 },
    { Assert-BdsInputEntry '/root.dll' 1 0 },
    { Assert-BdsInputEntry 'C:/escape.dll' 1 0 },
    { Assert-BdsInputEntry 'dir\escape.dll' 1 0 },
    { Assert-BdsInputEntry 'a//b' 1 0 },
    { Assert-BdsInputEntry './a' 1 0 },
    { Assert-BdsInputEntry 'dir./file' 1 0 },
    { Assert-BdsInputEntry 'dir /file' 1 0 },
    { Assert-BdsInputEntry 'a:stream' 1 0 },
    { Assert-BdsInputEntry 'NUL.dll' 1 0 },
    { Assert-BdsInputEntry 'dir/com1.txt' 1 0 },
    { Assert-BdsInputEntry 'dir/linked' 1 0xa0000000L },
    { Assert-BdsInputEntry 'dir/reparse' 1 0x400 },
    { Assert-BdsInputEntry 'huge' 67108865 0 },
    { Assert-BdsInputEntry 'negative' -1 0 },
    { Assert-BdsInputEntry 'dir/' 1 0 },
    { Assert-BdsPrelinkOutput 'bedrock_runtime_api.lib' 0 },
    { Assert-BdsPrelinkOutput 'unrelated.lib' 1392 },
    { $bad=$snapshot.psobject.Copy(); $bad.sha256='0'*64; Assert-BdsJsonSnapshotUnchanged $bad },
    { Assert-BdsInputIdentity $pins.archives[0] 1 $pins.archives[0].sha256 },
    { Assert-BdsInputIdentity $pins.archives[0] $pins.archives[0].size ('0'*64) },
    { $bad=$profile.psobject.Copy(); $bad.preloader_version='1.16.2'; Assert-BdsInputPins $pins $bad },
    { $bad=$pins.psobject.Copy(); $bad.archives=@($pins.archives[0]) * 6; Assert-BdsInputPins $bad $profile },
    { $bad=$pins.psobject.Copy(); $bad.schema_version=2; Assert-BdsInputPins $bad $profile },
    { Get-BdsRegularModuleImports @($imports -replace 'Symbol: \?needed@@','Symbol: ') 'PreLoader.dll' },
    { Get-BdsRegularModuleImports @($imports -replace 'PreLoader.dll','Other.dll') 'PreLoader.dll' },
    { Get-BdsRegularModuleImports ($imports + @('Import {','  Name: PreLoader.dll','  Symbol: x (0)','}')) 'PreLoader.dll' },
    { Get-BdsRegularModuleImports ($imports + @('Import {')) 'PreLoader.dll' }
)
foreach ($case in $cases) {
    $rejected=$false
    try { & $case | Out-Null } catch { $rejected=$true }
    if (-not $rejected) { throw "Expected official input rejection: $case" }
}
"Official input policy PASS ($($cases.Count) text-only rejections; no extraction/execution)"
