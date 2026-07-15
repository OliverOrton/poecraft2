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
$env:PYTHONPATH = "$Root/tools/ingest;$Root/bindings/python"
& $Python.Command @($Python.Prefix) -m unittest discover -s "$Root/tools/ingest/tests" -t "$Root/tools/ingest"
if ($LASTEXITCODE -ne 0) {
    throw "Python tests failed with exit code $LASTEXITCODE."
}

$env:PYTHONPATH = "$Root/tools/economy;$Root/tools/ingest;$Root/bindings/python"
& $Python.Command @($Python.Prefix) -m unittest discover `
    -s "$Root/tools/economy/tests" -t "$Root/tools/economy"
if ($LASTEXITCODE -ne 0) {
    throw "Economy tests failed with exit code $LASTEXITCODE."
}
$env:PYTHONPATH = "$Root/tools/ingest;$Root/tools/economy;$Root/bindings/python"

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

# Binding tests run after artifact compilation so fixture parity cannot
# accidentally exercise a stale generated dataset.
& $Python.Command @($Python.Prefix) -m unittest discover `
    -s "$Root/bindings/python/tests"
if ($LASTEXITCODE -ne 0) {
    throw "Python binding tests failed with exit code $LASTEXITCODE."
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

# Web/WASM acceptance checks use the generated Emscripten module and the same
# EngineClient/worker path as the browser application.
$Npm = Get-Command npm -ErrorAction SilentlyContinue
$WebPackage = "$Root/apps/web/package.json"
$WasmModule = "$Root/bindings/wasm/dist/poecraft_engine.mjs"
if ($Npm -and (Test-Path $WebPackage) -and (Test-Path $WasmModule)) {
    Push-Location "$Root/apps/web"
    try {
        & $Npm.Source test
        if ($LASTEXITCODE -ne 0) {
            throw "Web/WASM tests failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}
elseif (-not $Npm) {
    Write-Warning "npm was not found; web/WASM tests were skipped."
}
elseif (-not (Test-Path $WasmModule)) {
    Write-Warning "WASM module is absent; run scripts/build-wasm.ps1 before web tests."
}

Write-Host "Tests completed."
