# Read xmake's recorded data without invoking xmake, evaluating Lua, resolving
# packages, or modifying any SDK/cache. Unsupported metadata fails closed.
function Resolve-SdkInputs {
    param([Parameter(Mandatory)][string]$SdkRoot,
          [Parameter(Mandatory)][string]$PackagesRoot)
    $sdkPath = [IO.Path]::GetFullPath($SdkRoot).TrimEnd('\')
    $cachePath = [IO.Path]::GetFullPath($PackagesRoot).TrimEnd('\')
    $selected = [Collections.Generic.List[string]]::new()
    $definitions = [Collections.Generic.List[string]]::new()
    $metadata = Join-Path $sdkPath 'manifest.txt'
    if (Test-Path -LiteralPath $metadata -PathType Leaf) {
        $text = Get-Content -Raw -LiteralPath $metadata
        $deps = [regex]::Match($text, '(?ms)^    deps = \{\r?\n(?<body>.*?)^    \},?\r?$')
        if (-not $deps.Success) { throw 'SDK manifest has no supported dependency records' }
        $records = [regex]::Matches($deps.Groups['body'].Value,
            '(?ms)^        (?<key>[A-Za-z_][A-Za-z0-9_]*|\["[A-Za-z0-9_.+-]+"\]) = \{\r?\n(?<body>.*?)^        \},?\r?$')
        if ($records.Count -eq 0) { throw 'SDK dependency records are empty or unsupported' }
        # Reject unparsed text, including a function call in this data table.
        $residue = [regex]::Replace($deps.Groups['body'].Value,
            '(?ms)^        (?<key>[A-Za-z_][A-Za-z0-9_]*|\["[A-Za-z0-9_.+-]+"\]) = \{\r?\n(?<body>.*?)^        \},?\r?$', '')
        if (-not [string]::IsNullOrWhiteSpace($residue)) { throw 'Unsupported SDK dependency syntax' }
        foreach ($record in $records) {
            $name = $record.Groups['key'].Value.Trim('[',']','"')
            $body = $record.Groups['body'].Value
            $fields = [regex]::Matches($body, '(?m)^            (version|buildhash) = "([A-Za-z0-9_.+-]+)",?\r?$')
            if ($fields.Count -ne 2) { throw "Unsupported dependency pin for $name" }
            $values = @{}
            foreach ($field in $fields) {
                if ($values.ContainsKey($field.Groups[1].Value)) { throw "Duplicate dependency field for $name" }
                $values[$field.Groups[1].Value] = $field.Groups[2].Value
            }
            if (-not $values.version -or $values.buildhash -notmatch '^[a-f0-9]{32}$') {
                throw "Invalid dependency version/build hash for $name"
            }
            if (-not [string]::IsNullOrWhiteSpace([regex]::Replace($body,
                '(?m)^            (version|buildhash) = "([A-Za-z0-9_.+-]+)",?\r?$', ''))) {
                throw "Unsupported dependency pin fields for $name"
            }
            $selected.Add((Join-Path $cachePath "$($name.Substring(0,1))/$name/$($values.version)/$($values.buildhash)"))
        }
        $vars = [regex]::Match($text, '(?ms)^    vars = \{\r?\n(?<body>.*?)^    \},?\r?$')
        if (-not $vars.Success) { throw 'SDK manifest lacks exported compile definitions' }
        $defines = [regex]::Match($vars.Groups['body'].Value,
            '(?ms)^        defines = \{\r?\n(?<body>.*?)^        \},?\r?$')
        if (-not $defines.Success) { throw 'SDK manifest has unsupported compile definitions' }
        $defineMatches = [regex]::Matches($defines.Groups['body'].Value,
            '(?m)^            "([A-Z_][A-Z0-9_]*(?:=[A-Za-z0-9_.+-]+)?)",?\r?$')
        if (-not [string]::IsNullOrWhiteSpace([regex]::Replace($defines.Groups['body'].Value,
            '(?m)^            "([A-Z_][A-Z0-9_]*(?:=[A-Za-z0-9_.+-]+)?)",?\r?$', ''))) {
            throw 'Unsupported exported compile definition'
        }
        foreach ($define in $defineMatches) { $definitions.Add($define.Groups[1].Value) }
    } else {
        $metadata = Join-Path $sdkPath '.xmake/windows/x64/cache/references'
        if (-not (Test-Path -LiteralPath $metadata -PathType Leaf)) {
            throw 'Source SDK lacks exact xmake package references; refusing cache-wide include fallback'
        }
        $text = Get-Content -Raw -LiteralPath $metadata
        if ($text -notmatch '(?s)^\s*\{\s*packages\s*=\s*\{(?<paths>(?:\s*\[\[[^\r\n\]]+\]\]\s*,?)*?)\s*\},?\s*\}\s*$') {
            throw 'Unsupported xmake references syntax'
        }
        foreach ($match in [regex]::Matches($Matches['paths'], '\[\[([^\r\n\]]+)\]\]')) {
            $selected.Add($match.Groups[1].Value)
        }
        $recipePath = Join-Path $sdkPath 'xmake.lua'
        $recipe = Get-Content -Raw -LiteralPath $recipePath
        $public = [regex]::Matches($recipe, '(?s)add_defines\((?<body>[^()]*?\{public\s*=\s*true\}[^()]*)\)')
        if ($public.Count -eq 0) { throw 'Source SDK lacks supported public definition records' }
        foreach ($group in $public) {
            $body = $group.Groups['body'].Value
            $residue = [regex]::Replace($body, '\{public\s*=\s*true\}', '')
            $residue = [regex]::Replace($residue, '"([A-Z_][A-Z0-9_]*(?:=[A-Za-z0-9_.+-]+)?)"', '')
            if ($residue -notmatch '^[\s,]*$') { throw 'Unsupported source public definitions' }
            foreach ($match in [regex]::Matches($body, '"([A-Z_][A-Z0-9_]*(?:=[A-Za-z0-9_.+-]+)?)"')) {
                $definitions.Add($match.Groups[1].Value)
            }
        }
    }
    if ($selected.Count -eq 0) { throw 'SDK dependency closure is empty' }
    $includeDirs = [Collections.Generic.List[string]]::new()
    $dependencies = [Collections.Generic.List[object]]::new()
    $names = @{}
    foreach ($path in $selected) {
        $root = [IO.Path]::GetFullPath($path)
        if (-not $root.StartsWith($cachePath + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Recorded dependency is outside the selected read-only package cache: $root"
        }
        $relative = $root.Substring($cachePath.Length + 1)
        if ($relative -notmatch '^(?<letter>[A-Za-z0-9])\\(?<name>[A-Za-z0-9_.+-]+)\\(?<version>[A-Za-z0-9_.+-]+)\\(?<hash>[a-f0-9]{32})$') {
            throw "Invalid xmake dependency cache path: $root"
        }
        $name = $Matches.name
        $version = $Matches.version
        $hash = $Matches.hash
        if ($name -eq 'levilamina') { throw 'Another LeviLamina header tree cannot satisfy SDK dependencies' }
        if ($names.ContainsKey($name)) { throw "Duplicate dependency name/version selection: $name" }
        $names[$name] = $true
        $manifest = Join-Path $root 'manifest.txt'
        if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) { throw "Missing exact dependency: $root" }
        $packageText = Get-Content -Raw -LiteralPath $manifest
        $manifestName = [regex]::Match($packageText, '(?m)^    name = "([^"]+)",?\r?$')
        $manifestVersion = [regex]::Match($packageText, '(?m)^    version = "([^"]+)",?\r?$')
        if ($manifestName.Groups[1].Value -ne $name -or $manifestVersion.Groups[1].Value -ne $version) {
            throw "Dependency manifest does not match pinned cache path: $root"
        }
        $dependencies.Add([pscustomobject]@{ name=$name; version=$version; buildhash=$hash;
            root=$root; manifest_sha256=(Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash })
        $include = Join-Path $root 'include'
        if (Test-Path -LiteralPath $include -PathType Container) {
            if ((Test-Path -LiteralPath (Join-Path $include 'mc/_HeaderOutputPredefine.h')) -or
                (Test-Path -LiteralPath (Join-Path $include 'll/api/mod/RegisterHelper.h'))) {
                throw "Dependency shadows generated SDK headers: $include"
            }
            $includeDirs.Add($include)
            foreach ($suffix in @('stb','concurrentqueue/moodycamel')) {
                $subdir = Join-Path $include $suffix
                if (Test-Path -LiteralPath $subdir -PathType Container) { $includeDirs.Add($subdir) }
            }
        }
    }
    foreach ($define in $definitions) {
        if ($define -match '^LL_EXPORT(?:=|$)|^LL_PLAT_C(?:=|$)') {
            throw "SDK exports an incompatible library/client definition: $define"
        }
    }
    $macros = @{}
    foreach ($define in $definitions) {
        $key = ($define -split '=',2)[0]
        if ($macros.ContainsKey($key) -and $macros[$key] -ne $define) {
            throw "Conflicting SDK compile definitions for $key"
        }
        $macros[$key] = $define
    }
    if (-not ($definitions -match '^ENTT_PACKED_PAGE=') -or -not ($definitions -match '^ENTT_SPARSE_PAGE=')) {
        throw 'SDK is missing required public EnTT layout definitions'
    }
    return [pscustomobject]@{ metadata_path=$metadata;
        metadata_sha256=(Get-FileHash -LiteralPath $metadata -Algorithm SHA256).Hash;
        include_dirs=$includeDirs.ToArray(); defines=$definitions.ToArray(); dependencies=$dependencies.ToArray() }
}
