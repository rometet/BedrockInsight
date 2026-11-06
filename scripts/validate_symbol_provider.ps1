# Pinned cached static dependency used by official LL26.60 (not a BDS DLL).
function Get-BdsSymbolProvider([string]$PackageCache) {
    if (-not $PackageCache) { throw 'BEDROCKINSIGHT_XMAKE_PACKAGES is required' }
    $root=Join-Path $PackageCache 's/symbolprovider/v1.3.0/9731305ba7704f5982e5b034fbd5e46c'
    $library=Join-Path $root 'lib/SymbolProvider.lib'
    $manifest=Join-Path $root 'manifest.txt'
    $text=Get-Content -Raw -LiteralPath $manifest
    foreach ($pattern in @('name = "symbolprovider"','version = "v1.3.0"','arch = "x64"',
        'plat = "windows"','runtimes = "MD"','shared = false','toolchains = "clang-cl"')) {
        if (-not $text.Contains($pattern)) { throw "SymbolProvider manifest mismatch: $pattern" }
    }
    $hash=(Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash
    # Locally built cached artifact digest, NOT an upstream published binary hash.
    if ($hash -ine 'B569E4BB25EE66FB9439D1D4B42C2EDCEF41878222ED9684B4118D986301ABFB') {
        throw 'Unreviewed SymbolProvider cached static library; do not fall back to delayimp.lib'
    }
    [pscustomobject]@{path=$library;sha256=$hash;manifest=$manifest;
        manifest_sha256=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash;
        source_commit='6c93ec45c8455992ee726d92df60316c8e731c44';
        provenance='Cached x64 MD clang-cl v1.3.0 build; local digest, not upstream binary authentication'}
}
