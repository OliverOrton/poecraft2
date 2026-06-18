[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

function Get-PoeCraftPython {
    if ($env:POECRAFT_PYTHON) {
        return @{
            Command = $env:POECRAFT_PYTHON
            Prefix = @()
        }
    }
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) {
        return @{
            Command = $launcher.Source
            Prefix = @("-3")
        }
    }
    $command = Get-Command python -ErrorAction SilentlyContinue
    if ($command) {
        return @{
            Command = $command.Source
            Prefix = @()
        }
    }
    throw "Python was not found. Set POECRAFT_PYTHON to a Python 3.11+ executable."
}

$Python = Get-PoeCraftPython
& $Python.Command @($Python.Prefix) -m compileall -q "$Root/tools/ingest/poecraft_ingest"
if ($LASTEXITCODE -ne 0) {
    throw "Python compile smoke check failed with exit code $LASTEXITCODE."
}

$CMake = Get-Command cmake -ErrorAction SilentlyContinue
if ($CMake) {
    & $CMake.Source -S "$Root/engine" -B "$Root/build/engine" -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }
    & $CMake.Source --build "$Root/build/engine" --config Release
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}
else {
    $Compiler = Get-Command g++,clang++ -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($Compiler) {
        $BuildDirectory = "$Root/build/engine"
        New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
        & $Compiler.Source `
            -std=c++20 `
            "-I$Root/engine/include" `
            "$Root/engine/tests/header_smoke.cpp" `
            -o "$BuildDirectory/poecraft_header_smoke.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ header smoke build failed with exit code $LASTEXITCODE."
        }
    }
    else {
        Write-Warning "CMake and a C++20 compiler were not found; C++ header smoke build was skipped."
    }
}

Write-Host "Build completed."
