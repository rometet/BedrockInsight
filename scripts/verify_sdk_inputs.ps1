$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'resolve_sdk_inputs.ps1')
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$fixtureRoot = Join-Path $projectRoot ('.build/sdk-input-tests/' + [guid]::NewGuid().ToString('N'))
$cache = Join-Path $fixtureRoot 'packages'
$sdk = Join-Path $fixtureRoot 'sdk'
New-Item -ItemType Directory -Path $cache,$sdk -Force | Out-Null
function Assert-True([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
function Assert-Rejected([scriptblock]$Action,[string]$Pattern) {
    $caught = $false
    try { & $Action | Out-Null }
    catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $caught = $true
    }
    Assert-True $caught "Expected rejection: $Pattern"
}
function New-Package([string]$Version,[string]$Hash) {
    $root = Join-Path $cache "p/parallel-hashmap/$Version/$Hash"
    New-Item -ItemType Directory -Path (Join-Path $root 'include') -Force | Out-Null
    @"
{
    name = "parallel-hashmap",
    version = "$Version"
}
"@ | Set-Content -LiteralPath (Join-Path $root 'manifest.txt') -Encoding UTF8
    return $root
}
$old = New-Package 'v1.3.12' ('a' * 32)
$current = New-Package 'v2.0.0' ('b' * 32)
$missing = 'c' * 32
$manifest = @'
{
    deps = {
        ["parallel-hashmap"] = {
            buildhash = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            version = "v2.0.0"
        }
    },
    vars = {
        defines = {
            "ENTT_PACKED_PAGE=128",
            "ENTT_SPARSE_PAGE=4096",
            "ENTT_NO_MIXIN",
            "LL_PLAT_S"
        }
    }
}
'@
$manifestPath = Join-Path $sdk 'manifest.txt'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$resolved = Resolve-SdkInputs -SdkRoot $sdk -PackagesRoot $cache
Assert-True ($resolved.dependencies.Count -eq 1 -and $resolved.dependencies[0].version -eq 'v2.0.0') 'Version pin lost'
Assert-True ($resolved.include_dirs -contains (Join-Path $current 'include')) 'Pinned headers missing'
Assert-True ($resolved.include_dirs -notcontains (Join-Path $old 'include')) 'Unpinned cache header pollution'
Assert-True ($resolved.defines -contains 'ENTT_NO_MIXIN') 'Exported ABI definition lost'
$manifest.Replace('b' * 32,$missing) | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $sdk -PackagesRoot $cache } 'Missing exact dependency'
$manifest | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$manifest.Replace('ENTT_NO_MIXIN','ENTT_PACKED_PAGE=256') | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $sdk -PackagesRoot $cache } 'Conflicting SDK compile definitions'
$manifest.Replace('LL_PLAT_S','LL_PLAT_C') | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $sdk -PackagesRoot $cache } 'incompatible library/client definition'
$manifest.Replace('version = "v2.0.0"','version = "../v2.0.0"') | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $sdk -PackagesRoot $cache } 'Unsupported dependency pin'

$source = Join-Path $fixtureRoot 'source'
$cacheDir = Join-Path $source '.xmake/windows/x64/cache'
New-Item -ItemType Directory -Path $cacheDir -Force | Out-Null
$references = @"
{
    packages = {
        [[$current]]
    }
}
"@
$referencePath = Join-Path $cacheDir 'references'
$references | Set-Content -LiteralPath $referencePath -Encoding UTF8
@'
add_defines("LL_EXPORT")
add_defines(
    "ENTT_PACKED_PAGE=128",
    "ENTT_SPARSE_PAGE=4096",
    {public = true}
)
'@ | Set-Content -LiteralPath (Join-Path $source 'xmake.lua') -Encoding UTF8
$resolved = Resolve-SdkInputs -SdkRoot $source -PackagesRoot $cache
Assert-True ($resolved.dependencies[0].version -eq 'v2.0.0' -and $resolved.defines -notcontains 'LL_EXPORT') 'Source ABI input selection'
($references + 'os.execute("not data")') | Set-Content -LiteralPath $referencePath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $source -PackagesRoot $cache } 'Unsupported xmake references syntax'
$references.Replace($current,$sdk) | Set-Content -LiteralPath $referencePath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $source -PackagesRoot $cache } 'outside the selected'
$references.Replace("[[$current]]","[[$current]],`n        [[$old]]") |
    Set-Content -LiteralPath $referencePath -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $source -PackagesRoot $cache } 'Duplicate dependency'
$references | Set-Content -LiteralPath $referencePath -Encoding UTF8
$header = Join-Path $current 'include/mc/_HeaderOutputPredefine.h'
New-Item -ItemType Directory -Path (Split-Path $header) -Force | Out-Null
'shadow fixture' | Set-Content -LiteralPath $header -Encoding UTF8
Assert-Rejected { Resolve-SdkInputs -SdkRoot $source -PackagesRoot $cache } 'shadows generated SDK headers'
'BedrockInsight SDK input resolution tests: PASS'
"Fixtures: $fixtureRoot"
