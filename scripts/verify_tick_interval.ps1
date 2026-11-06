# No BDS launch or active-folder writes. Compile/run retained scratch fixtures.
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $project ('.build/tick-interval-tests/run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'tmp') -Force|Out-Null
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$arguments=@('/nologo','/EHsc','/MD','/std:c++20','/DNOMINMAX','/W4','/WX',
    "-I$(Join-Path $project 'src')","-I$(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'include')")
foreach($source in @('tests/tick_interval_tests.cpp','src/bedrock_insight/core/storage.cpp',
    'src/bedrock_insight/core/sqlite_api.cpp','src/bedrock_insight/core/event.cpp',
    'src/bedrock_insight/core/admin_api.cpp','src/bedrock_insight/core/rollback.cpp')) {
    $arguments+=Join-Path $project $source
}
$exe=Join-Path $root 'TickIntervalTests.exe'
$arguments+=@("/Fe:$exe",'/link',(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'))
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
Push-Location -LiteralPath $root
try {
    Invoke-BdsLoggedNative $compiler $arguments (Join-Path $root 'compile.log')
    Invoke-BdsLoggedNative $exe @() (Join-Path $root 'tests.log')
} finally {Pop-Location}
"Tick interval verification evidence: $root"
