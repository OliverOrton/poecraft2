[CmdletBinding()]
param(
    [ValidateSet("All", "Python", "Native", "Web")]
    [string]$Scope = "All",
    [switch]$SkipBuild,
    [switch]$FetchPinnedData
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/python-common.ps1"
$Python = Get-PoeCraftPython
$env:PYTHONPATH = "$Root/tools/ingest;$Root/bindings/python"
Write-Host "Validation scope: $Scope; Python: $($Python.Command)"

function Invoke-ProjectPython {
    param([string[]]$Arguments)
    & $Python.Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Python command failed ($LASTEXITCODE): $($Arguments -join ' ')"
    }
}

$NeedsPython = $Scope -in @("All", "Python")
$NeedsNative = $Scope -in @("All", "Native")
$NeedsWeb = $Scope -in @("All", "Web")
if (($NeedsPython -or $NeedsNative) -and -not $SkipBuild) {
    & "$PSScriptRoot/build.ps1"
}
$EngineTests = "$Root/build/engine/poecraft_engine_tests.exe"
$SolverBenchmark = "$Root/build/engine/poecraft_solver_benchmark.exe"
if ($NeedsPython -and -not (Test-Path "$Root/build/engine/poecraft_engine.dll")) {
    throw "Required native binding is unavailable; run scripts/build.ps1."
}
if (($NeedsPython -or $NeedsNative) -and -not (Test-Path $SolverBenchmark)) {
    throw "Required solver benchmark is unavailable (also needed by Python Lab tests); run scripts/build.ps1."
}
if ($NeedsNative -and -not (Test-Path $EngineTests)) {
    throw "Required native test executable is unavailable; run scripts/build.ps1."
}
if ($NeedsPython) {
    Invoke-ProjectPython @("-c", "import pytest; print('pytest', pytest.__version__)")
}

# Full frozen game data is required by these lanes. This is independent of
# economy refresh. Existing canonical data is validated, never silently replaced.
$LockPath = "$Root/fixtures/repoe/production-source-manifest.json"
$Lock = Get-Content -LiteralPath $LockPath -Raw | ConvertFrom-Json
$Source = "$Root/data/raw/repoe"
$Database = "$Root/data/sqlite/poecraft.db"
$Artifact = "$Root/data/compiled/current"
$ValidationOutput = "$Root/build/validation"
New-Item -ItemType Directory -Force -Path $ValidationOutput | Out-Null
if (-not (Test-Path -LiteralPath $Database)) {
    if ($FetchPinnedData) {
        Invoke-ProjectPython @("-m", "poecraft_ingest.cli", "fetch", "--output", $Source,
            "--locked-manifest", $LockPath)
    }
    foreach ($Entry in $Lock.files) {
        $SourceFile = Join-Path $Source $Entry.logical_name
        if (-not (Test-Path -LiteralPath $SourceFile) -or
            (Get-FileHash -LiteralPath $SourceFile -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Entry.content_hash) {
            throw "Required frozen source $($Entry.logical_name) is unavailable or mismatched. Use -FetchPinnedData for the pinned archive; current data is not a substitute."
        }
    }
    Invoke-ProjectPython @("-m", "poecraft_ingest.cli", "ingest", "--source", $Source,
        "--database", $Database, "--report", "$ValidationOutput/ingest.json")
}
Invoke-ProjectPython @("-m", "poecraft_ingest.cli", "validate", "--database", $Database)
Invoke-ProjectPython @("$Root/tools/ingest/validate_spec_fixtures.py", "--database", $Database,
    "--fixtures", "$Root/fixtures/spec")
if (-not (Test-Path -LiteralPath "$Artifact/manifest.json")) {
    Invoke-ProjectPython @("$Root/tools/ingest/compile_engine_data.py", "compile", "--database", $Database,
        "--output", $Artifact, "--timestamp-from-lock", $LockPath)
}
$ActualManifestHash = (Get-FileHash -LiteralPath "$Artifact/manifest.json" -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ActualManifestHash -ne $Lock.runtime_artifact.manifest_sha256) {
    # Preserve the actual bytes, not a ConvertFrom-Json timestamp rendering.
    Copy-Item -LiteralPath "$Artifact/manifest.json" -Destination "$ValidationOutput/runtime-identity-$ActualManifestHash.manifest.json"
    $PayloadIdentities = foreach ($Name in @("game-data.json", "strings.json")) {
        $PayloadPath = Join-Path $Artifact $Name
        $Present = Test-Path -LiteralPath $PayloadPath -PathType Leaf
        [ordered]@{ name = $Name; present = $Present
            sha256 = $(if ($Present) { (Get-FileHash -LiteralPath $PayloadPath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
            byte_size = $(if ($Present) { (Get-Item -LiteralPath $PayloadPath).Length } else { $null }) }
    }
    $Identity = [ordered]@{ powershell_version = $PSVersionTable.PSVersion.ToString()
        python = $Python.Command; python_version = (& $Python.Command --version)
        expected_manifest_sha256 = $Lock.runtime_artifact.manifest_sha256
        actual_manifest_sha256 = $ActualManifestHash; payloads = @($PayloadIdentities) }
    [System.IO.File]::WriteAllText("$ValidationOutput/runtime-identity-$ActualManifestHash.json",
        ($Identity | ConvertTo-Json -Depth 5), [System.Text.UTF8Encoding]::new($false))
    throw "The runtime manifest differs from the required frozen fixture. Preserve it and resolve the data identity before testing."
}
Invoke-ProjectPython @("$Root/tools/ingest/compile_engine_data.py", "validate", "--database", $Database,
    "--artifact", $Artifact)

if ($NeedsPython) {
    # Separate processes preserve the established tests.test_ingest fixture
    # import without colliding with the other packages' tests namespaces.
    Invoke-ProjectPython @("-m", "pytest", "$Root/tools/ingest/tests", "--import-mode=importlib", "-q")
    $env:PYTHONPATH = "$Root/tools/economy;$Root/tools/ingest;$Root/bindings/python"
    Invoke-ProjectPython @("-m", "pytest", "$Root/tools/economy/tests", "--import-mode=importlib", "-q")
    # Preserve the original unittest discovery root for test_stress's
    # top-level test_bindings fixture import, within this isolated process.
    $env:PYTHONPATH = "$Root/bindings/python/tests;$Root/bindings/python;$Root/tools/ingest;$Root/tools/economy"
    Invoke-ProjectPython @("-m", "pytest", "$Root/bindings/python/tests", "--import-mode=importlib", "-q")
}
if ($NeedsNative) {
    $CTest = Get-Command ctest -ErrorAction SilentlyContinue
    if ($CTest -and (Test-Path "$Root/build/engine/CMakeCache.txt")) {
        & $CTest.Source --test-dir "$Root/build/engine" -C Release --output-on-failure
    }
    else {
        & $EngineTests $Artifact "$Root/fixtures/spec"
    }
    if ($LASTEXITCODE -ne 0) { throw "C++ tests failed with exit code $LASTEXITCODE." }
    & $SolverBenchmark --artifact $Artifact --corpus "$Root/fixtures/solver-benchmarks/v1/manifest.json" --validate-only
    if ($LASTEXITCODE -ne 0) { throw "Solver corpus validation failed with exit code $LASTEXITCODE." }
}
if ($NeedsWeb) {
    $Npm = Get-Command npm -ErrorAction SilentlyContinue
    if (-not $Npm -or -not (Test-Path "$Root/bindings/wasm/dist/poecraft_engine.mjs") -or
        -not (Test-Path "$Root/bindings/wasm/dist/poecraft_engine.wasm")) {
        throw "Required web runtime is unavailable: npm and the release WASM module are required."
    }
    Push-Location "$Root/apps/web"
    try {
        foreach ($NpmArguments in @(@("ci"), @("run", "build:data"), @("test"), @("run", "typecheck"))) {
            & $Npm.Source @NpmArguments
            if ($LASTEXITCODE -ne 0) { throw "npm $NpmArguments failed with exit code $LASTEXITCODE." }
        }
    }
    finally { Pop-Location }
}
Write-Host "Validation completed for scope $Scope. Other scopes were not selected. Optional GUI coverage is reported by pytest."
