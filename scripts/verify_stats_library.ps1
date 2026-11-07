# Local fixture/HTTP/DOM regression only. NEVER starts BDS or a persistent viewer.
[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
$statsProject=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$statsEvidence=Join-Path $statsProject ('.build\stats-view-tests\run-'+[guid]::NewGuid().ToString('N'))
Assert-BdsUnredirectedAncestors $statsEvidence
New-Item -ItemType Directory -Path $statsEvidence | Out-Null
function Get-StatsInputHashes {
    $statsHashes=[ordered]@{}
    $statsFiles=@(Get-ChildItem (Join-Path $statsProject 'companion'),(Join-Path $statsProject 'tests') -Recurse -File |
        Where-Object {$_.Extension -in '.py','.js','.cjs','.html','.css','.json'})
    $statsFiles+=Get-Item $PSCommandPath,(Join-Path $PSScriptRoot 'invoke_logged_native.ps1')
    foreach($statsFile in ($statsFiles | Sort-Object FullName)) {
        $statsName=$statsFile.FullName.Substring($statsProject.Length+1).Replace('\','/')
        $statsHashes[$statsName]=(Get-FileHash -LiteralPath $statsFile.FullName -Algorithm SHA256).Hash
    }
    return $statsHashes
}
$statsPreviousPath=$env:PYTHONPATH
$statsPreviousUtf8=$env:PYTHONUTF8
$statsReceipt=[ordered]@{
    kind='stats-library-local-regression-v1';passed=$false
    scope='fixture SQLite -> authenticated ephemeral loopback HTTP -> Japanese JS DOM; not gameplay'
    native_dll_built=$false;bds_started=$false;persistent_viewer_started=$false;full_parity_accepted=$false
    before=$null;after=$null;source_hashes_match=$false;error=$null
}
Push-Location $statsProject
try {
    $env:PYTHONPATH='tests;.';$env:PYTHONUTF8='1'
    $statsReceipt.before=Get-StatsInputHashes
    Invoke-BdsLoggedNative -Executable 'python' -Arguments @('-W','error::ResourceWarning','-m','unittest','discover','-s','tests','-p','test_*.py','-v') -LogPath (Join-Path $statsEvidence 'python-tests.log')
    Invoke-BdsLoggedNative -Executable 'python' -Arguments @('-O','-m','unittest','discover','-s','tests','-p','test_parity_inventory.py','-v') -LogPath (Join-Path $statsEvidence 'optimized-inventory.log')
    $statsReceipt.after=Get-StatsInputHashes
    $statsReceipt.source_hashes_match=(ConvertTo-Json -InputObject $statsReceipt.before -Compress) -ceq (ConvertTo-Json -InputObject $statsReceipt.after -Compress)
    if(-not $statsReceipt.source_hashes_match){throw 'Source inputs changed across regression; do not claim latest-tree PASS'}
    $statsReceipt.passed=$true
    Get-Content (Join-Path $statsEvidence 'python-tests.log') -Tail 5
    Write-Output ('Source hash guard PASS: '+$statsReceipt.before.Count+' captured inputs match before/after')
} catch {
    $statsReceipt.error=$_.Exception.Message
    throw
} finally {
    $statsReceipt | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $statsEvidence 'regression.json') -Encoding utf8
    Write-Output ('Local regression evidence: '+$statsEvidence)
    $env:PYTHONPATH=$statsPreviousPath;$env:PYTHONUTF8=$statsPreviousUtf8
    Pop-Location
}
