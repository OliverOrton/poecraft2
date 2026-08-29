$PoeCraftKnownCMake = @(
    "C:\msys64\ucrt64\bin\cmake.exe",
    "C:\Program Files\CMake\bin\cmake.exe"
)
$PoeCraftKnownNinja = @("C:\msys64\ucrt64\bin\ninja.exe")
$PoeCraftKnownCCompiler = @("C:\msys64\ucrt64\bin\gcc.exe")
$PoeCraftKnownCxxCompiler = @("C:\msys64\ucrt64\bin\g++.exe")

function Find-PoeCraftTool {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [string[]]$ExplicitPaths = @(),
        [string[]]$KnownPaths = @()
    )

    foreach ($ExplicitPath in $ExplicitPaths) {
        if (-not $ExplicitPath) {
            continue
        }
        if (-not (Test-Path -LiteralPath $ExplicitPath -PathType Leaf)) {
            throw "Configured tool path does not exist: $ExplicitPath"
        }
        return [System.IO.Path]::GetFullPath($ExplicitPath)
    }
    foreach ($Name in $Names) {
        $Command = Get-Command $Name -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($Command) {
            return $Command.Source
        }
    }
    foreach ($KnownPath in $KnownPaths) {
        if ($KnownPath -and (Test-Path -LiteralPath $KnownPath)) {
            return $KnownPath
        }
    }
    return $null
}

function Find-PoeCraftVisualStudioBundledTool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelativePath
    )

    if (-not ${env:ProgramFiles(x86)}) {
        return $null
    }
    $VsWhere = Join-Path ${env:ProgramFiles(x86)} `
        "Microsoft Visual Studio/Installer/vswhere.exe"
    if (-not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        return $null
    }
    $InstallPath = & $VsWhere -latest -products * -property installationPath |
        Select-Object -First 1
    if (-not $InstallPath) {
        return $null
    }
    $Candidate = Join-Path $InstallPath $RelativePath
    if (Test-Path -LiteralPath $Candidate -PathType Leaf) {
        return [System.IO.Path]::GetFullPath($Candidate)
    }
    return $null
}

function Find-PoeCraftCMake {
    $Tool = Find-PoeCraftTool -Names @("cmake") `
        -ExplicitPaths @($env:POECRAFT_CMAKE) `
        -KnownPaths $PoeCraftKnownCMake
    if ($Tool) {
        return $Tool
    }
    return Find-PoeCraftVisualStudioBundledTool `
        -RelativePath "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
}

function Find-PoeCraftNinja {
    $Tool = Find-PoeCraftTool -Names @("ninja") `
        -ExplicitPaths @($env:POECRAFT_NINJA) `
        -KnownPaths $PoeCraftKnownNinja
    if ($Tool) {
        return $Tool
    }
    return Find-PoeCraftVisualStudioBundledTool `
        -RelativePath "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
}

function Find-PoeCraftCCompiler {
    return Find-PoeCraftTool -Names @("gcc", "clang") `
        -ExplicitPaths @($env:POECRAFT_CC) `
        -KnownPaths $PoeCraftKnownCCompiler
}

function Find-PoeCraftCompiler {
    return Find-PoeCraftTool -Names @("g++", "clang++") `
        -ExplicitPaths @($env:POECRAFT_CXX) `
        -KnownPaths $PoeCraftKnownCxxCompiler
}

function Find-PoeCraftCcache {
    return Find-PoeCraftTool -Names @("ccache") `
        -KnownPaths @("C:\msys64\ucrt64\bin\ccache.exe")
}

function ConvertTo-PoeCraftCMakePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    return [System.IO.Path]::GetFullPath($Path).Replace("\", "/")
}

function Get-PoeCraftEngineSources {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Root
    )

    $Manifest = Join-Path $Root "engine/engine-sources.txt"
    if (-not (Test-Path -LiteralPath $Manifest)) {
        throw "Canonical engine source inventory not found at $Manifest."
    }

    $Entries = @(
        Get-Content -LiteralPath $Manifest |
            ForEach-Object { $_.Trim() } |
            Where-Object { $_ -and -not $_.StartsWith("#") }
    )
    if ($Entries.Count -eq 0) {
        throw "Canonical engine source inventory is empty: $Manifest"
    }
    if (($Entries | Select-Object -Unique).Count -ne $Entries.Count) {
        throw "Canonical engine source inventory contains duplicate entries."
    }

    $Sources = @()
    foreach ($Entry in $Entries) {
        $Source = [System.IO.Path]::GetFullPath((Join-Path "$Root/engine" $Entry))
        if (-not (Test-Path -LiteralPath $Source)) {
            throw "Engine source inventory entry does not exist: $Entry"
        }
        $Sources += $Source
    }

    $Discovered = @(
        Get-ChildItem -Path "$Root/engine/src" -Recurse -Filter *.cpp -File |
            ForEach-Object { $_.FullName } |
            Sort-Object
    )
    $Inventory = @($Sources | Sort-Object)
    $Difference = @(Compare-Object -ReferenceObject $Inventory `
        -DifferenceObject $Discovered)
    if ($Difference.Count -ne 0) {
        $Details = ($Difference | ForEach-Object {
            "$($_.SideIndicator) $($_.InputObject)"
        }) -join [Environment]::NewLine
        throw "Canonical engine source inventory does not match engine/src:`n$Details"
    }

    return $Sources
}
