$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $projectRoot
$buildRoot = Join-Path $projectRoot 'build\manual'
New-Item -ItemType Directory -Path (Join-Path $buildRoot 'tmp') -Force | Out-Null
$env:TEMP = Join-Path $buildRoot 'tmp'
$env:TMP = $env:TEMP
$compiler = Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$arguments = @('/nologo','/EHsc','/MD','/std:c++20','/DNOMINMAX','/W4','/WX','/DBEDROCKINSIGHT_FAKE_SDK',
    "-I$(Join-Path $projectRoot 'tests/fake_sdk')","-I$(Join-Path $projectRoot 'src')",
    "-I$(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'include')")
$sources = @('tests/native_runtime_tests.cpp','tests/fake_busy_tick.cpp','tests/fake_player_actions.cpp','tests/fake_player_death.cpp','tests/fake_player_pickup.cpp','tests/fake_player_jump.cpp','tests/fake_player_kills.cpp','tests/fake_player_hurt.cpp','tests/fake_player_block.cpp','src/bedrock_insight/levilamina/player_modes.cpp','src/bedrock_insight/levilamina/runtime.cpp',
    'src/bedrock_insight/core/storage.cpp','src/bedrock_insight/core/sqlite_api.cpp',
    'src/bedrock_insight/core/event.cpp','src/bedrock_insight/core/admin_api.cpp','src/bedrock_insight/core/rollback.cpp')
foreach ($source in $sources) { $arguments += Join-Path $projectRoot $source }
$exe = Join-Path $buildRoot 'NativeRuntimeSafetyTests.exe'
$arguments += @("/Fe:$exe",'/link',(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'))
Push-Location -LiteralPath $buildRoot
try {
    & $compiler @arguments
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally { Pop-Location }
& $exe
exit $LASTEXITCODE
