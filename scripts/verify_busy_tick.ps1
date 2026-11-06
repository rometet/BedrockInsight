$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $project ('.build/busy-tick-tests/run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'tmp') -Force|Out-Null
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$exe=Join-Path $root 'BusyTickTests.exe'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
Push-Location -LiteralPath $root
try {
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/busy_tick_tests.cpp'),"/Fe:$exe") (Join-Path $root 'compile.log')
    Invoke-BdsLoggedNative $exe @() (Join-Path $root 'tests.log')
}finally{Pop-Location}
"Busy math evidence: $root"
