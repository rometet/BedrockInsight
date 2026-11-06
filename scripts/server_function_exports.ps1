. (Join-Path $PSScriptRoot 'validate_server_release.ps1')
function Get-BdsServerFunctionExports([string[]]$Inspection) {
    $allNames = @(Get-BdsPeNamedExports $Inspection)
    $text = $Inspection -join "`n"
    $directory = @([regex]::Matches($text,'(?m)^    ExportTableRVA: (0x[0-9A-Fa-f]+)$'))
    $size = @([regex]::Matches($text,'(?m)^    ExportTableSize: (0x[0-9A-Fa-f]+)$'))
    if ($directory.Count -ne 1 -or $size.Count -ne 1) { throw 'Missing/duplicate PE export directory' }
    $directoryStart = [Convert]::ToUInt64($directory[0].Groups[1].Value.Substring(2),16)
    $directorySize = [Convert]::ToUInt64($size[0].Groups[1].Value.Substring(2),16)
    if ($directoryStart -eq 0 -or $directorySize -eq 0 -or $directoryStart + $directorySize -gt 4294967296) {
        throw 'Invalid PE export directory range'
    }
    $sections = @()
    foreach ($record in [regex]::Matches($text,'(?ms)^  Section \{\n(?<body>.*?)^  \}')) {
        $body = $record.Groups['body'].Value
        $fields = @{}
        foreach ($field in @('VirtualAddress','VirtualSize','RawDataSize')) {
            $values = @([regex]::Matches($body,"(?m)^    ${field}: (0x[0-9A-Fa-f]+|[0-9]+)$"))
            if ($values.Count -ne 1) { throw "Missing/duplicate PE section field: $field" }
            $value = $values[0].Groups[1].Value
            $fields[$field] = if ($value.StartsWith('0x')) { [Convert]::ToUInt64($value.Substring(2),16) } else { [UInt64]::Parse($value) }
        }
        $start = $fields.VirtualAddress; $length = $fields.VirtualSize
        if ($start -eq 0 -or $length -eq 0 -or $start + $length -gt 4294967296) { throw 'Invalid PE section range' }
        foreach ($previous in $sections) {
            if ($start -lt $previous.end -and $previous.start -lt $start + $length) { throw 'Overlapping PE sections' }
        }
        $sections += [pscustomobject]@{
            start=$start;end=($start + $length);backed_end=($start + [Math]::Min($length,$fields.RawDataSize))
            code=($body -match '(?m)^      IMAGE_SCN_CNT_CODE ')
            execute=($body -match '(?m)^      IMAGE_SCN_MEM_EXECUTE ')
            read=($body -match '(?m)^      IMAGE_SCN_MEM_READ ')
            write=($body -match '(?m)^      IMAGE_SCN_MEM_WRITE ')
        }
    }
    if ($sections.Count -eq 0) { throw 'Missing PE sections' }
    $functions = @(); $excluded = @()
    foreach ($record in [regex]::Matches($text,'(?ms)^Export \{\n(?<body>.*?)^\}')) {
        $body = $record.Groups['body'].Value
        $name = [regex]::Match($body,'(?m)^  Name: (.+)$').Groups[1].Value
        $rva = [Convert]::ToUInt64([regex]::Match($body,'(?m)^  RVA: 0x([0-9A-Fa-f]+)$').Groups[1].Value,16)
        $containing = @($sections | Where-Object { $rva -ge $_.start -and $rva -lt $_.end })
        if ($containing.Count -ne 1) { throw 'Export RVA outside a unique PE section' }
        $section = $containing[0]
        $reason = $null
        if ($rva -ge $directoryStart -and $rva -lt $directoryStart + $directorySize) { $reason='forwarder' }
        elseif (-not $section.code -or -not $section.execute -or -not $section.read -or $section.write) { $reason='not_readonly_executable_code' }
        elseif ($rva -ge $section.backed_end) { $reason='not_file_backed' }
        if ($reason) { $excluded += [pscustomobject]@{name=$name;rva=$rva;reason=$reason}; continue }
        # Restricted MSVC/C export spelling; no .def aliases, directives,
        # comments, whitespace, quoting or generated code is accepted.
        if ($name -cnotmatch '^[A-Za-z_?@$][A-Za-z0-9_?@$]*$') { throw 'Unsupported function export spelling for .def' }
        $functions += [pscustomobject]@{name=$name;rva=$rva;section_start=$section.start;section_end=$section.end}
    }
    if ($functions.Count -eq 0 -or $functions.Count + $excluded.Count -ne $allNames.Count) { throw 'Incomplete function export classification' }
    return [pscustomobject]@{functions=$functions;excluded=$excluded;full_sdk=$false;implementation_code_generated=$false}
}
