$PoeCraftKnownCMake =
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$PoeCraftKnownNinja =
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
$PoeCraftKnownCompiler = "C:\msys64\ucrt64\bin\g++.exe"

function Find-PoeCraftTool {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Names,
        [string[]]$KnownPaths = @()
    )

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

function Find-PoeCraftCMake {
    return Find-PoeCraftTool -Names @("cmake") -KnownPaths @($PoeCraftKnownCMake)
}

function Find-PoeCraftNinja {
    return Find-PoeCraftTool -Names @("ninja") -KnownPaths @($PoeCraftKnownNinja)
}

function Find-PoeCraftCompiler {
    return Find-PoeCraftTool -Names @("g++", "clang++") `
        -KnownPaths @($PoeCraftKnownCompiler)
}

function Find-PoeCraftCcache {
    return Find-PoeCraftTool -Names @("ccache") `
        -KnownPaths @("C:\msys64\ucrt64\bin\ccache.exe")
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
