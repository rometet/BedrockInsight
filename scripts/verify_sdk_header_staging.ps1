$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script = Join-Path $PSScriptRoot 'stage_pinned_sdk_headers.ps1'
$snapshot = Join-Path $projectRoot '.build/pinned-sdk-headers-26.60.0'
$official = Get-Content -Raw -LiteralPath (Join-Path $snapshot 'catalog.json') | ConvertFrom-Json
$fixture = Join-Path $projectRoot ('.build/sdk-header-tests/' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixture -Force | Out-Null
$path = Join-Path $fixture 'catalog.json'
$good = [pscustomobject]@{schema_version=1;source_commit=$official.source_commit;
    files=@($official.files | Where-Object { $_.path -ceq 'src/mc/_HeaderOutputPredefine.h' })} |
    ConvertTo-Json -Depth 5
$good | Set-Content -LiteralPath $path -Encoding UTF8
& $script -CatalogPath $path -ValidateOnly
# A structurally valid subset must never be allowed to stage headers merely
# because it labels itself with the official commit. Import requires the full
# pinned catalog digest; ValidateOnly cannot copy or normalize source files.
$digestRejected = $false
try { & $script -CatalogPath $path | Out-Null }
catch {
    if ($_.Exception.Message -notmatch 'catalog digest does not match') { throw }
    $digestRejected = $true
}
if (-not $digestRejected) { throw 'Unpinned catalog was accepted for import' }
$cases = @(
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src/mc/../escape.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src\mc\escape.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src-client/mc/Client.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src/mc/C:/escape.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src/mc/alias./Block.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].path='src/mc/NUL.h'}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files[0].sha='z' * 40}},
    @{pattern='Invalid or duplicate';change={param($d) $d.files += $d.files[0]}},
    @{pattern='does not match';change={param($d) $d.source_commit='0' * 40}}
)
foreach ($case in $cases) {
    $document = $good | ConvertFrom-Json
    & $case.change $document
    $document | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $path -Encoding UTF8
    $rejected = $false
    try { & $script -CatalogPath $path -ValidateOnly | Out-Null }
    catch {
        if ($_.Exception.Message -notmatch $case.pattern) { throw }
        $rejected = $true
    }
    if (-not $rejected) { throw 'Expected SDK header path/profile rejection' }
}
'SDK header staging path/profile tests: PASS (10 rejection cases; no header copies)'
