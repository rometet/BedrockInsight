$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'server_function_exports.ps1')
# Text-only LLVM metadata fixtures, never fake DLL/SDK files.
$pe=@('Format: COFF-x86-64','  Machine: IMAGE_FILE_MACHINE_AMD64 (0x8664)','    IMAGE_FILE_DLL (0x2000)',
    '    ExportTableRVA: 0x3300','    ExportTableSize: 0x100',
    '  Section {','    VirtualAddress: 0x1000','    VirtualSize: 0x1000','    RawDataSize: 4096',
    '      IMAGE_SCN_CNT_CODE (0x20)','      IMAGE_SCN_MEM_EXECUTE (0x20000000)','      IMAGE_SCN_MEM_READ (0x40000000)','  }',
    '  Section {','    VirtualAddress: 0x3000','    VirtualSize: 0x1000','    RawDataSize: 4096',
    '      IMAGE_SCN_MEM_READ (0x40000000)','  }',
    'Export {','  Name: ?function@@','  RVA: 0x1100','}',
    'Export {','  Name: ?data@@','  RVA: 0x3100','}',
    'Export {','  Name: ?forward@@','  RVA: 0x3350','}')
$result=Get-BdsServerFunctionExports $pe
if ($result.functions.Count -ne 1 -or $result.functions[0].name -cne '?function@@' -or
    $result.excluded.Count -ne 2 -or $result.full_sdk -or $result.implementation_code_generated) { throw 'Function classification mismatch' }
$cases=@(
    { Get-BdsServerFunctionExports @($pe -replace 'COFF-x86-64','COFF-i386') },
    { Get-BdsServerFunctionExports @($pe | Where-Object {$_ -notmatch 'ExportTableRVA'}) },
    { Get-BdsServerFunctionExports ($pe + '    ExportTableSize: 0x100') },
    { Get-BdsServerFunctionExports @($pe -replace 'ExportTableSize: 0x100','ExportTableSize: 0x0') },
    { Get-BdsServerFunctionExports @($pe -replace 'ExportTableRVA: 0x3300','ExportTableRVA: 0xFFFFFFFF') },
    { Get-BdsServerFunctionExports @($pe -replace 'VirtualAddress: 0x3000','VirtualAddress: 0x1800') },
    { Get-BdsServerFunctionExports @($pe | Where-Object {$_ -notmatch 'VirtualSize'}) },
    { Get-BdsServerFunctionExports @($pe -replace 'VirtualSize: 0x1000','VirtualSize: 0x0') },
    { Get-BdsServerFunctionExports @($pe | Where-Object {$_ -notmatch 'IMAGE_SCN_MEM_EXECUTE'}) },
    { Get-BdsServerFunctionExports @($pe -replace 'IMAGE_SCN_MEM_EXECUTE','IMAGE_SCN_MEM_WRITE') },
    { Get-BdsServerFunctionExports @($pe -replace 'RawDataSize: 4096','RawDataSize: 1') },
    { Get-BdsServerFunctionExports @($pe -replace 'RVA: 0x1100','RVA: 0x9999') },
    { Get-BdsServerFunctionExports @($pe -replace '\?function@@','function=evil') },
    { Get-BdsServerFunctionExports @($pe -replace 'ExportTableRVA: 0x3300','ExportTableRVA: 0x1100') }
)
foreach($case in $cases){$rejected=$false;try{& $case|Out-Null}catch{$rejected=$true};if(-not $rejected){throw "Expected export rejection: $case"}}
"Server function export policy: PASS ($($cases.Count) text-only rejection cases; no binary fixtures)"
