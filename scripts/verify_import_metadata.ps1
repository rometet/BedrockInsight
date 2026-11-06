$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'validate_import_metadata.ps1')
# Textual inspection fixtures, not fake archive/DLL files.
$good = @('Format: COFF-x86-64','    Name: .debug$S (00)',
    'Format: COFF-x86-64','    Name: .idata$2 (00)','Format: COFF-x86-64',
    'File: LeviLamina.dll','Format: COFF-import-file-x86-64','Type: code',
    'Name type: name','Export name: example','Symbol: __imp_example')
$result = Assert-NamedLlImportMetadata $good
if ($result.server_sdk_verified -or $result.named_import_records -ne 1) { throw 'Inspection is not server SDK provenance' }
$cases = @(
    @{pattern='Unexpected section';lines=$good + '    Name: .text (00)'},
    @{pattern='Unexpected section';lines=$good + '    Name: .drectve (00)'},
    @{pattern='Executable section';lines=$good + '      IMAGE_SCN_MEM_EXECUTE'},
    @{pattern='archive format';lines=@($good -replace 'COFF-import-file-x86-64','COFF-import-file-i386')},
    @{pattern='named LeviLamina';lines=@($good -replace 'Name type: name','Name type: ordinal')},
    @{pattern='named LeviLamina';lines=@($good -replace 'File: LeviLamina.dll','File: Other.dll')},
    @{pattern='named LeviLamina';lines=@($good | Where-Object { $_ -notmatch '^Export name:' })},
    @{pattern='archive format';lines=@($good | Where-Object { $_ -cne 'Format: COFF-import-file-x86-64' })}
)
foreach ($case in $cases) {
    $caught=$false
    try { Assert-NamedLlImportMetadata $case.lines | Out-Null }
    catch { if ($_.Exception.Message -notmatch $case.pattern) { throw }; $caught=$true }
    if (-not $caught) { throw "Expected import metadata rejection: $($case.pattern)" }
}
'Import metadata inspection tests: PASS (8 rejections; text fixtures only, no SDK/BDS proof)'
