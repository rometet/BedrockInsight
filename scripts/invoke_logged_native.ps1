. (Join-Path $PSScriptRoot 'validate_bds_isolation.ps1')
function Invoke-BdsLoggedNative([string]$Executable,[string[]]$Arguments,[string]$LogPath) {
    $root=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
    $log=[IO.Path]::GetFullPath($LogPath)
    if(-not $log.StartsWith($root + '\.build\',[StringComparison]::OrdinalIgnoreCase)){throw 'Native log must stay in project .build'}
    Assert-BdsUnredirectedAncestors $log
    # Local preference: PS5 must not turn compiler stderr warnings into an
    # early terminating error. PowerShell writes native status in GLOBAL scope;
    # a local LASTEXITCODE variable would shadow it and falsely report null.
    $ErrorActionPreference='Continue'
    $global:LASTEXITCODE=$null
    try { & $Executable @Arguments *> $log }
    catch { throw "Native command failed to start: $($_.Exception.Message)" }
    $nativeExit=$global:LASTEXITCODE
    if($null -eq $nativeExit -or $nativeExit -ne 0){throw "Native command failed ($nativeExit): $log"}
}
