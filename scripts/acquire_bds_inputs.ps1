# Download only two pinned official dependency archives, never install/extract
# or run them. SDK headers/import library and BDS still have to be supplied.
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$profile = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'packaging/bds-profile.json') | ConvertFrom-Json
$downloadRoot = Join-Path $projectRoot '.build/bds-downloads'
try {
    # Refuse an existing redirected build/download directory.
    foreach ($directory in @((Join-Path $projectRoot '.build'),$downloadRoot)) {
        if (Test-Path -LiteralPath $directory) {
            if ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Refusing redirected download directory: $directory"
            }
        }
    }
    New-Item -ItemType Directory -Path $downloadRoot -Force | Out-Null
    foreach ($download in $profile.downloads) {
        if ($download.name -cnotmatch '^[A-Za-z0-9_.-]+\.zip$' -or
            $download.sha256 -cnotmatch '^[a-f0-9]{64}$' -or
            $download.url -cnotlike 'https://github.com/LiteLDev/*') { throw 'Invalid pinned archive' }
        $archive = Join-Path $downloadRoot $download.name
        if (-not (Test-Path -LiteralPath $archive)) {
            # A failed transfer remains .partial, never a verified .zip. No
            # proxy/security settings are changed and each attempt is bounded.
            $partial = "$archive.partial"
            if (Test-Path -LiteralPath $partial) { throw "Existing partial archive retained; inspect before retry: $partial" }
            & curl.exe --fail --location --connect-timeout 5 --max-time 60 --output $partial $download.url
            if ($LASTEXITCODE -ne 0) { throw "Archive acquisition failed: curl exit $LASTEXITCODE ($($download.name))" }
            $candidate = $partial
        } else { $candidate = $archive }
        if ((Get-Item -LiteralPath $candidate).Length -ne $download.size -or
            (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash -ine $download.sha256) {
            throw "Archive integrity mismatch: $candidate; not extracted or used"
        }
        if ($candidate -ne $archive) { Move-Item -LiteralPath $candidate -Destination $archive }
        Write-Output "VERIFIED ARCHIVE: $archive"
    }
    'Dependency archive acquisition complete; SDK/header matching and isolated BDS acceptance still required.'
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 2
}
