$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fixture=Join-Path $root ('.build/native-exit-tests/' + [guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $fixture
New-Item -ItemType Directory -Path $fixture|Out-Null
$shell=Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe'
function Encoded-Exit([int]$Code) {
    [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes("[Console]::Error.WriteLine('test stderr');exit $Code"))
}
Invoke-BdsLoggedNative $shell @('-NoProfile','-EncodedCommand',(Encoded-Exit 0)) (Join-Path $fixture 'success.log')
if((Get-Content -Raw (Join-Path $fixture 'success.log')) -notmatch 'test stderr'){throw 'Expected captured native stderr'}
foreach($case in @(
    @{exe=$shell;arguments=@('-NoProfile','-EncodedCommand',(Encoded-Exit 7))},
    @{exe=(Join-Path $fixture 'does-not-exist.exe');arguments=@()}
)){
    $rejected=$false
    try{Invoke-BdsLoggedNative $case.exe $case.arguments (Join-Path $fixture 'failure.log')}
    catch{if($_.Exception.Message -notmatch 'Native command failed'){throw};$rejected=$true}
    if(-not $rejected){throw 'Expected nonzero/missing native command rejection'}
}
if($ErrorActionPreference -cne 'Stop'){throw 'Native helper leaked its error preference'}
'Logged native command policy: PASS (stderr/exit0 success; exit7 and missing executable rejected; no binary fixtures)'
