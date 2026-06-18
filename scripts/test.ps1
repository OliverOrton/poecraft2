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

# Build/validate the canonical database derivatives before the engine tests run,
# so the engine data-loader suite has a complete runtime artifact to load.
$Database = "$Root/data/sqlite/poecraft.db"
$Artifact = "$Root/data/compiled/current"
if (Test-Path $Database) {
    & $Python.Command @($Python.Prefix) -m poecraft_ingest.cli validate --database $Database
    if ($LASTEXITCODE -ne 0) {
        throw "Canonical database validation failed with exit code $LASTEXITCODE."
    }

    & $Python.Command @($Python.Prefix) `
        "$Root/tools/ingest/validate_spec_fixtures.py" `
        --database $Database `
        --fixtures "$Root/fixtures/spec"
    if ($LASTEXITCODE -ne 0) {
        throw "Spec fixture validation failed with exit code $LASTEXITCODE."
    }

    & $Python.Command @($Python.Prefix) `
        "$Root/tools/ingest/compile_engine_data.py" `
        compile `
        --database $Database `
        --output $Artifact
    if ($LASTEXITCODE -ne 0) {
        throw "Complete runtime data compilation failed with exit code $LASTEXITCODE."
    }
    & $Python.Command @($Python.Prefix) `
        "$Root/tools/ingest/compile_engine_data.py" `
        validate `
        --database $Database `
        --artifact $Artifact
    if ($LASTEXITCODE -ne 0) {
        throw "Compiled data validation failed with exit code $LASTEXITCODE."
    }
}

# Engine tests. Prefer CTest (CMake build); fall back to the g++ test binary,
# then to the header smoke test. The data-loader suite needs the artifact path.
$CTest = Get-Command ctest -ErrorAction SilentlyContinue
$EngineTests = "$Root/build/engine/poecraft_engine_tests.exe"
$HeaderSmoke = "$Root/build/engine/poecraft_header_smoke.exe"
if ($CTest -and (Test-Path "$Root/build/engine/CMakeCache.txt")) {
    & $CTest.Source --test-dir "$Root/build/engine" -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "C++ tests failed with exit code $LASTEXITCODE."
    }
}
elseif (Test-Path $EngineTests) {
    $Fixtures = "$Root/fixtures/spec"
    if (Test-Path $Artifact) {
        & $EngineTests $Artifact $Fixtures
    }
    else {
        & $EngineTests
    }
    if ($LASTEXITCODE -ne 0) {
        throw "C++ engine tests failed with exit code $LASTEXITCODE."
    }
}
elseif (Test-Path $HeaderSmoke) {
    & $HeaderSmoke
    if ($LASTEXITCODE -ne 0) {
        throw "C++ header smoke test failed with exit code $LASTEXITCODE."
    }
}

Write-Host "Tests completed."
