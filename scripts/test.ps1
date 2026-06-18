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
$env:PYTHONPATH = "$Root/tools/ingest"
& $Python.Command @($Python.Prefix) -m unittest discover -s "$Root/tools/ingest/tests" -t "$Root/tools/ingest"
if ($LASTEXITCODE -ne 0) {
    throw "Python tests failed with exit code $LASTEXITCODE."
}

$CTest = Get-Command ctest -ErrorAction SilentlyContinue
if ($CTest -and (Test-Path "$Root/build/engine")) {
    & $CTest.Source --test-dir "$Root/build/engine" -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "C++ tests failed with exit code $LASTEXITCODE."
    }
}
elseif (Test-Path "$Root/build/engine/poecraft_header_smoke.exe") {
    & "$Root/build/engine/poecraft_header_smoke.exe"
    if ($LASTEXITCODE -ne 0) {
        throw "C++ header smoke test failed with exit code $LASTEXITCODE."
    }
}

$Database = "$Root/data/sqlite/poecraft.db"
if (Test-Path $Database) {
    & $Python.Command @($Python.Prefix) -m poecraft_ingest.cli validate --database $Database
    if ($LASTEXITCODE -ne 0) {
        throw "Canonical database validation failed with exit code $LASTEXITCODE."
    }
}

Write-Host "Tests completed."
