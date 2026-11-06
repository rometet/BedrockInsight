$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'validate_bds_profile.ps1')
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profile = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/bds-profile.json') | ConvertFrom-Json
$native = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'manifest.json') | ConvertFrom-Json
$tooth = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/tooth.json') | ConvertFrom-Json
$inputs = [pscustomobject]@{
    dependencies = @(
        [pscustomobject]@{name='bedrockdata';version='v26.60.6-server.5'},
        [pscustomobject]@{name='prelink';version='v0.8.8'})
    defines = @('ENTT_PACKED_PAGE=128','ENTT_SPARSE_PAGE=4096')
}
function Assert-True([bool]$Value,[string]$Message) { if (-not $Value) { throw $Message } }
function Assert-Rejected([scriptblock]$Action,[string]$Pattern) {
    $caught = $false
    try { & $Action }
    catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $caught = $true
    }
    Assert-True $caught "Expected rejection: $Pattern"
}
function Check-Profile { Assert-BdsProfileInputs $inputs $profile $native $tooth }
Check-Profile
foreach ($version in @('v26.51.1-server.12','v26.60.5-client.5','v26.60.6-server.6')) {
    $inputs.dependencies[0].version = $version
    Assert-Rejected { Check-Profile } 'bedrockdata requires exactly'
}
$inputs.dependencies[0].version = 'v26.60.6-server.5'
$inputs.dependencies[1].version = 'v0.8.7'
Assert-Rejected { Check-Profile } 'prelink requires exactly'
$inputs.dependencies[1].version = 'v0.8.8'
$inputs.dependencies += $inputs.dependencies[0]
Assert-Rejected { Check-Profile } 'bedrockdata requires exactly'
$inputs.dependencies = $inputs.dependencies[0..1]
$inputs.defines[0] = 'ENTT_PACKED_PAGE=256'
Assert-Rejected { Check-Profile } 'ABI definition mismatch'
$inputs.defines[0] = 'ENTT_PACKED_PAGE=128'
$native.extraInfo.requiredLeviLamina = '26.60.*'
Assert-Rejected { Check-Profile } 'Native manifest'
$native.extraInfo.requiredLeviLamina = '26.60.0'
$native.dependencies = @([pscustomobject]@{name='LeviLamina';version='26.60.0'})
Assert-Rejected { Check-Profile } 'Native manifest'
$native.dependencies = @()
$tooth.variants[0].assets[0].placements[0].dest = 'plugins/BedrockInsight/'
Assert-Rejected { Check-Profile } 'Tooth manifest'
$tooth.variants[0].assets[0].placements[0].dest = 'mods/BedrockInsight/'
Check-Profile
Assert-True ($profile.levilamina_commit -ceq '072b9ffe0e9d9e0ec846f11d294880e892fe7770') 'Official release commit changed'
Assert-True ($profile.bds_version -ceq '1.26.60.6' -and $profile.preloader_version -ceq '1.16.7') 'Runtime profile changed'
foreach ($download in $profile.downloads) {
    Assert-True ($download.sha256 -cmatch '^[a-f0-9]{64}$' -and $download.size -gt 0) 'Invalid official archive digest/size'
    Assert-True ($download.url -clike 'https://github.com/LiteLDev/*') 'Unexpected download host'
}
$recipe = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'xmake.lua')
Assert-True ($recipe.Contains('add_requires("levilamina 26.60.0", {configs = {target_type = "server"}})')) 'xmake version/variant drift'
'BedrockInsight BDS profile tests: PASS (metadata only; no SDK/BDS load)'
