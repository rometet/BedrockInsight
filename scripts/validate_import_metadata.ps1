# LLVM textual inspection policy for a diagnostic link, NOT server provenance.
function Assert-NamedLlImportMetadata([string[]]$Inspection) {
    $formats = @($Inspection | Where-Object { $_ -match '^Format:' })
    $imports = @($formats | Where-Object { $_ -ceq 'Format: COFF-import-file-x86-64' }).Count
    $descriptors = @($formats | Where-Object { $_ -ceq 'Format: COFF-x86-64' }).Count
    if ($imports -eq 0 -or $descriptors -ne 3 -or $formats.Count -ne $imports + $descriptors) {
        throw 'Unsupported import archive format/descriptor count'
    }
    if (@($Inspection | Where-Object { $_ -match 'IMAGE_SCN_(MEM_EXECUTE|CNT_CODE)' }).Count) {
        throw 'Executable section in reference import archive'
    }
    foreach ($line in $Inspection) {
        if ($line -match '^    Name: ([^ ]+)') {
            if ($Matches[1] -cnotin @('.debug$S','.idata$2','.idata$3','.idata$4','.idata$5','.idata$6')) {
                throw "Unexpected section in reference import archive: $($Matches[1])"
            }
        }
    }
    $nameTypes = @($Inspection | Where-Object { $_ -match '^Name type:' })
    $dllNames = @($Inspection | Where-Object { $_ -ceq 'File: LeviLamina.dll' })
    $exports = @($Inspection | Where-Object { $_ -match '^Export name: .+' })
    if ($nameTypes.Count -ne $imports -or $dllNames.Count -ne $imports -or $exports.Count -ne $imports -or
        @($nameTypes | Where-Object { $_ -cne 'Name type: name' }).Count) {
        throw 'Reference imports must be named LeviLamina.dll records, not ordinal imports'
    }
    return [pscustomobject]@{named_import_records=$imports;descriptor_records=$descriptors;
        executable_sections=$false;server_sdk_verified=$false}
}
