# No BDS/DLL loading. A standalone synthetic-import EXE tests the real helper.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'validate_symbol_provider.ps1')
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$provider=Get-BdsSymbolProvider $env:BEDROCKINSIGHT_XMAKE_PACKAGES
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$dlltool=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-dlltool.exe'
$reader=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'llvm-readobj.exe'
$root=Join-Path $project ('.build/runtime-delay-tests/run-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$definition=Join-Path $root 'probe.def'
@('LIBRARY bedrock_runtime.dll','EXPORTS','bedrock_insight_delay_probe') |
    Set-Content -LiteralPath $definition -Encoding ASCII
$import=Join-Path $root 'probe.lib'
Invoke-BdsLoggedNative $dlltool @('-m','i386:x86-64','-D','bedrock_runtime.dll','-d',$definition,'-l',$import) (Join-Path $root 'dlltool.log')
$exe=Join-Path $root 'RuntimeDelayResolutionTests.exe'
$argsList=@('/nologo','/EHsc','/MD','/O2','/std:c++20','/utf-8',
    (Join-Path $project 'tests/runtime_delay_resolution_tests.cpp'),"/Fe:$exe",'/link',
    $import,$provider.path,'/DELAYLOAD:bedrock_runtime.dll','/NODEFAULTLIB:delayimp.lib')
Push-Location -LiteralPath $root
try { Invoke-BdsLoggedNative $compiler $argsList (Join-Path $root 'compile.log') }
finally { Pop-Location }
Invoke-BdsLoggedNative $reader @('--coff-imports',$exe) (Join-Path $root 'imports.log')
Invoke-BdsLoggedNative $exe @() (Join-Path $root 'tests.log')
$control=Join-Path $root 'MsvcDelayNegativeControl.exe'
$controlArgs=@('/nologo','/EHsc','/MD','/O2','/std:c++20','/utf-8',
    '/DBEDROCKINSIGHT_TEST_MSVC_DELAY_CONTROL',
    (Join-Path $project 'tests/runtime_delay_resolution_tests.cpp'),"/Fe:$control",'/link',
    $import,'delayimp.lib','/DELAYLOAD:bedrock_runtime.dll')
Push-Location -LiteralPath $root
try { Invoke-BdsLoggedNative $compiler $controlArgs (Join-Path $root 'control-compile.log') }
finally { Pop-Location }
Invoke-BdsLoggedNative $control @() (Join-Path $root 'control-tests.log')
$provider | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'provider.json') -Encoding UTF8
Get-Content -LiteralPath (Join-Path $root 'tests.log')
Get-Content -LiteralPath (Join-Path $root 'control-tests.log')
"Evidence: $root"
