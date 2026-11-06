$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $project ('.build/player-action-tests/run-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'tmp') -Force|Out-Null
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$exe=Join-Path $root 'DropObservationTests.exe'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
Push-Location -LiteralPath $root
try {
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/drop_observation_tests.cpp'),"/Fe:$exe") (Join-Path $root 'compile.log')
    Invoke-BdsLoggedNative $exe @() (Join-Path $root 'tests.log')
    $stateExe=Join-Path $root 'StateTransitionTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/state_transition_tests.cpp'),"/Fe:$stateExe") (Join-Path $root 'state-compile.log')
    Invoke-BdsLoggedNative $stateExe @() (Join-Path $root 'state-tests.log')
    $modeExe=Join-Path $root 'PlayerModeTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'tests/fake_sdk')","-I$(Join-Path $project 'src')",
        (Join-Path $project 'tests/player_mode_tests.cpp'),(Join-Path $project 'src/bedrock_insight/levilamina/player_modes.cpp'),
        "/Fe:$modeExe") (Join-Path $root 'mode-compile.log')
    Invoke-BdsLoggedNative $modeExe @() (Join-Path $root 'mode-tests.log')
    $pickupExe=Join-Path $root 'PickupObservationTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/pickup_observation_tests.cpp'),"/Fe:$pickupExe") (Join-Path $root 'pickup-compile.log')
    Invoke-BdsLoggedNative $pickupExe @() (Join-Path $root 'pickup-tests.log')
    $jumpExe=Join-Path $root 'JumpObservationTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/jump_observation_tests.cpp'),"/Fe:$jumpExe") (Join-Path $root 'jump-compile.log')
    Invoke-BdsLoggedNative $jumpExe @() (Join-Path $root 'jump-tests.log')
    $killExe=Join-Path $root 'DirectKillTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/direct_kill_tests.cpp'),"/Fe:$killExe") (Join-Path $root 'kill-compile.log')
    Invoke-BdsLoggedNative $killExe @() (Join-Path $root 'kill-tests.log')
    $useExe=Join-Path $root 'UseObservationTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/use_observation_tests.cpp'),"/Fe:$useExe") (Join-Path $root 'use-compile.log')
    Invoke-BdsLoggedNative $useExe @() (Join-Path $root 'use-tests.log')
    $hurtExe=Join-Path $root 'HurtObservationTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/hurt_observation_tests.cpp'),"/Fe:$hurtExe") (Join-Path $root 'hurt-compile.log')
    Invoke-BdsLoggedNative $hurtExe @() (Join-Path $root 'hurt-tests.log')
    $blockExe=Join-Path $root 'BlockObservationTests.exe'
    Invoke-BdsLoggedNative $compiler @('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX',
        "-I$(Join-Path $project 'src')",(Join-Path $project 'tests/block_observation_tests.cpp'),"/Fe:$blockExe") (Join-Path $root 'block-compile.log')
    Invoke-BdsLoggedNative $blockExe @() (Join-Path $root 'block-tests.log')
}finally{Pop-Location}
"Drop boundary evidence: $root"
