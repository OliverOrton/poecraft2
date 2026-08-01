[CmdletBinding()]
param(
    [ValidateSet(
        "Configure", "Engine", "Tests", "Benchmark", "Test", "TestAll",
        "Full", "BenchmarkValidate", "RerunFailed", "CleanRebuild")]
    [string]$Task = "Engine",
    [ValidateSet(
        "abstract", "automatic-eldritch", "calc", "compile", "eval", "api",
        "feasibility", "imprint", "policy-refinement", "refinement", "s8-3",
        "solve")]
    [string]$Suite = "refinement",
    [int]$Jobs = 0
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/engine-build-common.ps1"

$CMake = Find-PoeCraftCMake
$Ninja = Find-PoeCraftNinja
if (-not $CMake -or -not $Ninja) {
    throw "The canonical development path requires the installed CMake and Ninja tools."
}

$BuildDirectory = "$Root/build/engine"
$Artifact = "$Root/data/compiled/current"
$Fixtures = "$Root/fixtures/spec"
$ParallelArgs = @("--parallel")
if ($Jobs -gt 0) {
    $ParallelArgs += $Jobs
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(ValueFromRemainingArguments = $true)]
        [object[]]$Arguments
    )
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Command failed with exit code $LASTEXITCODE."
    }
}

function Invoke-Configure {
    $ConfigureArgs = @("--preset", "ucrt64-ninja-release")
    $Ccache = Find-PoeCraftCcache
    if ($Ccache) {
        Write-Host "Using optional compiler cache: $Ccache"
        $CcacheCMakePath = $Ccache.Replace("\", "/")
        $ConfigureArgs += "-DCMAKE_C_COMPILER_LAUNCHER=$CcacheCMakePath"
        $ConfigureArgs += "-DCMAKE_CXX_COMPILER_LAUNCHER=$CcacheCMakePath"
    }
    else {
        $ConfigureArgs += "-DCMAKE_C_COMPILER_LAUNCHER="
        $ConfigureArgs += "-DCMAKE_CXX_COMPILER_LAUNCHER="
    }
    Push-Location "$Root/engine"
    try {
        Invoke-Checked $CMake @ConfigureArgs
    }
    finally {
        Pop-Location
    }
}

function Ensure-Configured {
    if (-not (Test-Path -LiteralPath "$BuildDirectory/build.ninja")) {
        Invoke-Configure
    }
}

function Invoke-BuildPreset {
    param([string]$Preset)
    Ensure-Configured
    Push-Location "$Root/engine"
    try {
        Invoke-Checked $CMake --build --preset $Preset @ParallelArgs
    }
    finally {
        Pop-Location
    }
}

function Get-SuiteArguments {
    switch ($Suite) {
        "abstract" { return @("--solver-abstract-only", $Artifact) }
        "automatic-eldritch" { return @("--solver-automatic-eldritch-only") }
        "calc" { return @("--solver-calc-only", $Artifact) }
        "compile" { return @("--solver-compile-only", $Artifact) }
        "eval" { return @("--solver-eval-only", $Artifact) }
        "api" { return @("--solver-api-only", $Artifact) }
        "feasibility" { return @("--solver-feasibility-only", $Artifact) }
        "imprint" { return @("--solver-imprint-only", $Artifact) }
        "policy-refinement" { return @("--solver-policy-refinement-only") }
        "refinement" { return @("--solver-refinement-only") }
        "s8-3" { return @("--solver-s8-3-only") }
        "solve" { return @("--solver-solve-only", $Artifact) }
    }
}

switch ($Task) {
    "Configure" {
        Invoke-Configure
    }
    "Engine" {
        Invoke-BuildPreset "engine-only"
    }
    "Tests" {
        Invoke-BuildPreset "tests-only"
    }
    "Benchmark" {
        Invoke-BuildPreset "benchmark-only"
    }
    "Test" {
        Invoke-BuildPreset "tests-only"
        $SuiteArguments = @(Get-SuiteArguments)
        Invoke-Checked "$BuildDirectory/poecraft_engine_tests.exe" @SuiteArguments
    }
    "TestAll" {
        Invoke-BuildPreset "tests-only"
        $CTestArgs = @(
            "--test-dir", $BuildDirectory, "--output-on-failure", "-L", "native",
            "--parallel"
        )
        if ($Jobs -gt 0) {
            $CTestArgs += $Jobs
        }
        $CTest = Join-Path ([System.IO.Path]::GetDirectoryName($CMake)) "ctest.exe"
        Invoke-Checked $CTest @CTestArgs
    }
    "Full" {
        Invoke-BuildPreset "tests-only"
        Invoke-Checked "$BuildDirectory/poecraft_engine_tests.exe" `
            $Artifact $Fixtures
    }
    "BenchmarkValidate" {
        Invoke-BuildPreset "benchmark-only"
        Invoke-Checked "$BuildDirectory/poecraft_solver_benchmark.exe" `
            --artifact $Artifact `
            --corpus "$Root/fixtures/solver-benchmarks/v1/manifest.json" `
            --validate-only
    }
    "RerunFailed" {
        Ensure-Configured
        $CTest = Join-Path ([System.IO.Path]::GetDirectoryName($CMake)) "ctest.exe"
        $CTestArgs = @(
            "--test-dir", $BuildDirectory, "--rerun-failed",
            "--output-on-failure", "--parallel"
        )
        if ($Jobs -gt 0) {
            $CTestArgs += $Jobs
        }
        Invoke-Checked $CTest @CTestArgs
    }
    "CleanRebuild" {
        if (Test-Path -LiteralPath $BuildDirectory) {
            $ResolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
            $ExpectedBuild = [System.IO.Path]::GetFullPath("$Root/build/engine")
            if ($ResolvedBuild -ne $ExpectedBuild) {
                throw "Refusing to remove unexpected build path: $ResolvedBuild"
            }
            Remove-Item -LiteralPath $ResolvedBuild -Recurse -Force
        }
        Invoke-Configure
        Invoke-BuildPreset "all-native"
    }
}
