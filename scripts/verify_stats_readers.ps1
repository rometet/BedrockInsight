# Only own new project-local fixtures/binaries, no BDS or original DB reads.
$ErrorActionPreference='Stop'
$project=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$root=Join-Path $project ('.build/stats-reader-tests/run-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $root 'tmp')|Out-Null
$env:TEMP=Join-Path $root 'tmp';$env:TMP=$env:TEMP
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$compiler=Join-Path $env:BEDROCKINSIGHT_CLANGCL_ROOT 'clang-cl.exe'
$common=@('/nologo','/EHsc','/MD','/std:c++20','/W4','/WX','/DNOMINMAX',
    "-I$(Join-Path $project 'src')","-I$(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'include')")
Push-Location -LiteralPath $root
try {
    foreach($test in @('stats_reader_tests','remote_api_tests','killed_by_storage_tests')) {
        $exe=Join-Path $root ($test+'.exe')
        $argsNative=$common+@((Join-Path $project "tests/$test.cpp"),
            (Join-Path $project 'src/bedrock_insight/core/stats_reader.cpp'),
            (Join-Path $project 'src/bedrock_insight/core/sqlite_api.cpp'))
        if($test -eq 'remote_api_tests'){$argsNative+=Join-Path $project 'src/bedrock_insight/core/remote_api.cpp'}
        if($test -eq 'killed_by_storage_tests') {
            foreach($source in @('event','storage','rollback','admin_api')) {
                $argsNative+=Join-Path $project "src/bedrock_insight/core/$source.cpp"
            }
        }
        $argsNative+=@("/Fe:$exe",'/link',(Join-Path $env:BEDROCKINSIGHT_SQLITE_ROOT 'lib/sqlite3.lib'))
        Invoke-BdsLoggedNative $compiler $argsNative (Join-Path $root ($test+'-compile.log'))
        Invoke-BdsLoggedNative $exe @() (Join-Path $root ($test+'.log'))
        Get-Content -LiteralPath (Join-Path $root ($test+'.log'))
    }
}finally{Pop-Location}
"Stats reader evidence: $root (fixtures only; not BDS/other-Mod acceptance)"
