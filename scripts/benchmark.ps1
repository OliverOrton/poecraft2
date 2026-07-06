[CmdletBinding()]
param(
    [int]$Runs = 100000
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Output = Join-Path $Root "build/performance"
New-Item -ItemType Directory -Force -Path $Output | Out-Null

& "$Root/scripts/build.ps1"
if ($LASTEXITCODE -ne 0) {
    throw "Native build failed with exit code $LASTEXITCODE."
}

$env:PYTHONPATH = "$Root/bindings/python"
foreach ($ActionsPerRun in @(1, 10)) {
    foreach ($Action in @("alteration", "chaos")) {
        & py -3 "$Root/tools/benchmark_engine.py" `
            --artifact "$Root/data/compiled/current" `
            --runs $Runs `
            --action $Action `
            --actions-per-run $ActionsPerRun `
            --output "$Output/native-$Action-$ActionsPerRun-actions.json"
        if ($LASTEXITCODE -ne 0) {
            throw "Native $Action benchmark failed with exit code $LASTEXITCODE."
        }
    }
}

& "$Root/scripts/build-wasm.ps1"
if ($LASTEXITCODE -ne 0) {
    throw "WASM build failed with exit code $LASTEXITCODE."
}

Push-Location "$Root/apps/web"
try {
    $env:POECRAFT_BENCH_RUNS = "$Runs"
    $env:POECRAFT_BENCH_PROFILE = "ui"
    foreach ($ActionsPerRun in @(1, 10)) {
        $env:POECRAFT_BENCH_ACTIONS_PER_RUN = "$ActionsPerRun"
        foreach ($Action in @("alteration", "chaos")) {
            $env:POECRAFT_BENCH_ACTION = $Action
            $env:POECRAFT_BENCH_OUTPUT = "$Output/wasm-$Action-$ActionsPerRun-actions.json"
            & npm run benchmark
            if ($LASTEXITCODE -ne 0) {
                throw "WASM $Action benchmark failed with exit code $LASTEXITCODE."
            }
        }
    }
}
finally {
    Remove-Item Env:POECRAFT_BENCH_ACTION -ErrorAction SilentlyContinue
    Remove-Item Env:POECRAFT_BENCH_ACTIONS_PER_RUN -ErrorAction SilentlyContinue
    Remove-Item Env:POECRAFT_BENCH_OUTPUT -ErrorAction SilentlyContinue
    Remove-Item Env:POECRAFT_BENCH_PROFILE -ErrorAction SilentlyContinue
    Remove-Item Env:POECRAFT_BENCH_RUNS -ErrorAction SilentlyContinue
    Pop-Location
}

Write-Host "Benchmark reports written to $Output"
