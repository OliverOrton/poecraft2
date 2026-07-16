[CmdletBinding()]
param(
    [ValidateSet("all", "native", "wasm")]
    [string]$Runner = "all",
    [string]$Case = "",
    [ValidatePattern("^[A-Za-z0-9._-]+$")]
    [string]$Label = "s7.0-unoptimized",
    [switch]$SkipBuild,
    [switch]$SkipVerification
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Corpus = Join-Path $Root "fixtures/solver-benchmarks/v1/manifest.json"
$Output = Join-Path $Root "build/performance"
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$ReportSuffix = "$Label-v1"
if ($Case) {
    if ($Case -notmatch "^[A-Za-z0-9._-]+$") {
        throw "Benchmark case id contains unsupported characters: $Case"
    }
    $ReportSuffix = "$Label-case-$Case-v1"
}
$NativeReport = Join-Path $Output "native-solver-$ReportSuffix.json"
$WasmReport = Join-Path $Output "wasm-worker-solver-$ReportSuffix.json"
$ComparisonReport = Join-Path $Output "solver-$ReportSuffix-comparison.json"

if (-not (Test-Path -LiteralPath $Corpus)) {
    throw "Solver benchmark corpus not found: $Corpus"
}

if ($Runner -in @("all", "native")) {
    if (-not $SkipBuild) {
        & "$Root/scripts/build.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "Native build failed with exit code $LASTEXITCODE."
        }
    }

    $Native = Get-ChildItem -LiteralPath "$Root/build/engine" -Recurse `
        -Filter "poecraft_solver_benchmark.exe" | Select-Object -First 1
    if (-not $Native) {
        throw "Native solver benchmark executable was not built."
    }
    $NativeArgs = @(
        "--artifact", "$Root/data/compiled/current",
        "--corpus", $Corpus,
        "--output", $NativeReport
    )
    if ($Case) { $NativeArgs += @("--case", $Case) }
    if ($SkipVerification) { $NativeArgs += "--skip-verification" }
    & $Native.FullName @NativeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Native solver benchmark failed with exit code $LASTEXITCODE."
    }
}

if ($Runner -in @("all", "wasm")) {
    if (-not $SkipBuild) {
        & "$Root/scripts/build-wasm.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "WASM build failed with exit code $LASTEXITCODE."
        }
    }

    Push-Location "$Root/apps/web"
    try {
        $WasmArgs = @(
            "--corpus", $Corpus,
            "--output", $WasmReport
        )
        if ($Case) { $WasmArgs += @("--case", $Case) }
        if ($SkipVerification) { $WasmArgs += "--skip-verification" }
        & npm run benchmark:solver -- @WasmArgs
        if ($LASTEXITCODE -ne 0) {
            throw "WASM solver benchmark failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

if ($Runner -eq "all") {
    & py -3 "$Root/scripts/compare-solver-benchmarks.py" `
        --native $NativeReport `
        --wasm $WasmReport `
        --output $ComparisonReport
    if ($LASTEXITCODE -ne 0) {
        throw "Native/WASM solver comparison failed with exit code $LASTEXITCODE."
    }
}

Write-Host "Solver benchmark reports written to $Output"
