[CmdletBinding()]
param(
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) {
    $OutputDirectory = Join-Path $Root "dist/python"
}

& (Join-Path $PSScriptRoot "build.ps1")
if ($LASTEXITCODE -ne 0) {
    throw "Native build failed with exit code $LASTEXITCODE."
}

$PackageDirectory = Join-Path $Root "bindings/python/poecraft_engine"
$LibraryCandidates = @(
    (Join-Path $Root "build/engine/poecraft_engine.dll"),
    (Join-Path $Root "build/engine/Release/poecraft_engine.dll"),
    (Join-Path $Root "build/engine/libpoecraft_engine.so"),
    (Join-Path $Root "build/engine/libpoecraft_engine.dylib")
)
$Library = $LibraryCandidates |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $Library) {
    throw "The shared poecraft engine library was not produced."
}

$GeneratedNames = @(
    "poecraft_engine.dll",
    "libpoecraft_engine.so",
    "libpoecraft_engine.dylib",
    "libwinpthread-1.dll"
)
foreach ($Name in $GeneratedNames) {
    $Target = Join-Path $PackageDirectory $Name
    if (Test-Path -LiteralPath $Target) {
        Remove-Item -LiteralPath $Target -Force
    }
}
Copy-Item -LiteralPath $Library -Destination $PackageDirectory -Force

$Runtime = Join-Path (Split-Path -Parent $Library) "libwinpthread-1.dll"
if (Test-Path -LiteralPath $Runtime) {
    Copy-Item -LiteralPath $Runtime -Destination $PackageDirectory -Force
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Get-ChildItem -LiteralPath $OutputDirectory -Filter "poecraft_engine-*.whl" |
    Remove-Item -Force

function Get-PoeCraftPython {
    if ($env:POECRAFT_PYTHON) {
        return @{
            Command = $env:POECRAFT_PYTHON
            Prefix = @()
        }
    }
    $Launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($Launcher) {
        return @{
            Command = $Launcher.Source
            Prefix = @("-3")
        }
    }
    $Command = Get-Command python -ErrorAction SilentlyContinue
    if ($Command) {
        return @{
            Command = $Command.Source
            Prefix = @()
        }
    }
    throw "Python was not found."
}

$Python = Get-PoeCraftPython
& $Python.Command @($Python.Prefix) -m pip wheel `
    (Join-Path $Root "bindings/python") `
    --no-deps `
    --no-build-isolation `
    --wheel-dir $OutputDirectory
if ($LASTEXITCODE -ne 0) {
    throw "Python wheel build failed with exit code $LASTEXITCODE."
}

foreach ($Name in $GeneratedNames) {
    $Target = Join-Path $PackageDirectory $Name
    if (Test-Path -LiteralPath $Target) {
        Remove-Item -LiteralPath $Target -Force
    }
}

Write-Host "Python wheel written to $OutputDirectory"
