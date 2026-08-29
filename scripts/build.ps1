[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/engine-build-common.ps1"

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
& $Python.Command @($Python.Prefix) -m compileall -q "$Root/tools/economy/poecraft_economy"
if ($LASTEXITCODE -ne 0) {
    throw "Economy Python compile smoke check failed with exit code $LASTEXITCODE."
}

$HarvestGeneratedDirectory = "$Root/build/engine/generated"
$HarvestGeneratedHeader = "$HarvestGeneratedDirectory/harvest_crafts.generated.hpp"
& $Python.Command @($Python.Prefix) `
    "$Root/scripts/generate-harvest-crafts.py" `
    --recipes "$Root/fixtures/economy/harvest-recipes-v1.json" `
    --output $HarvestGeneratedHeader
if ($LASTEXITCODE -ne 0) {
    throw "Harvest craft allowlist generation failed with exit code $LASTEXITCODE."
}

$CMake = Find-PoeCraftCMake
$Ninja = Find-PoeCraftNinja
$CCompiler = Find-PoeCraftCCompiler
$Compiler = Find-PoeCraftCompiler
if ($CMake -and $Ninja -and $CCompiler -and $Compiler) {
    $Ccache = Find-PoeCraftCcache
    $ConfigureArgs = @(
        "--preset", "ucrt64-ninja-release",
        "-DCMAKE_MAKE_PROGRAM=$(ConvertTo-PoeCraftCMakePath $Ninja)",
        "-DCMAKE_C_COMPILER=$(ConvertTo-PoeCraftCMakePath $CCompiler)",
        "-DCMAKE_CXX_COMPILER=$(ConvertTo-PoeCraftCMakePath $Compiler)"
    )
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
        & $CMake @ConfigureArgs
    }
    finally {
        Pop-Location
    }
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }
    & $CMake --build "$Root/build/engine" --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Warning @"
FAST CMAKE/NINJA PATH UNAVAILABLE.
Falling back to direct g++ commands that recompile every engine source for
each output. This path is intentionally retained for portability but is not
the normal development workflow. Use scripts/dev-engine.ps1 when the checked
toolchain is installed.
"@
    if ($Compiler) {
        $BuildDirectory = "$Root/build/engine"
        New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
        & $Compiler `
            -std=c++20 `
            "-I$Root/engine/include" `
            "$Root/engine/tests/header_smoke.cpp" `
            -o "$BuildDirectory/poecraft_header_smoke.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ header smoke build failed with exit code $LASTEXITCODE."
        }

        $EngineSources = Get-PoeCraftEngineSources -Root $Root
        $TestSources = @(
            "$Root/engine/tests/test_main.cpp",
            "$Root/engine/tests/test_bitset.cpp",
            "$Root/engine/tests/test_rng.cpp",
            "$Root/engine/tests/test_item_state.cpp",
            "$Root/engine/tests/test_blocking.cpp",
            "$Root/engine/tests/test_data_loader.cpp",
            "$Root/engine/tests/test_session_builder.cpp",
            "$Root/engine/tests/test_actions.cpp",
            "$Root/engine/tests/test_bestiary.cpp",
            "$Root/engine/tests/test_simulator.cpp",
            "$Root/engine/tests/test_solver_abstract.cpp",
            "$Root/engine/tests/test_solver_calc.cpp",
            "$Root/engine/tests/test_solver_solve.cpp",
            "$Root/engine/tests/test_solver_compile.cpp",
            "$Root/engine/tests/test_solver_s8_3.cpp",
            "$Root/engine/tests/test_solver_eval.cpp",
            "$Root/engine/tests/test_solver_api.cpp",
            "$Root/engine/tests/test_solver_refinement.cpp",
            "$Root/engine/benchmarks/solver_executable_fragment.cpp",
            "$Root/engine/benchmarks/solver_executable_fragment_engine.cpp",
            "$Root/engine/tests/test_solver_fragment.cpp",
            "$Root/engine/tests/test_solver_quotient_proof.cpp",
            "$Root/engine/tests/test_solver_quotient_partition.cpp"
        )
        # -static-libstdc++/-static-libgcc avoid a ld.bfd crash (exit 116) seen
        # in MSYS2 ucrt64 binutils when linking the shared C++ runtime against
        # exception-bearing COMDAT sections. They also make the test binary
        # self-contained. CMake/MSVC builds do not need this.
        & $Compiler `
            -std=c++20 -O2 -ffp-contract=off `
            -static-libstdc++ -static-libgcc `
            "-I$Root/engine/include" `
            "-I$HarvestGeneratedDirectory" `
            @EngineSources `
            @TestSources `
            -o "$BuildDirectory/poecraft_engine_tests.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ engine test build failed with exit code $LASTEXITCODE."
        }

        & $Compiler `
            -std=c++20 -O2 -ffp-contract=off -shared `
            -static-libstdc++ -static-libgcc `
            "-Wl,--export-all-symbols" `
            "-I$Root/engine/include" `
            "-I$HarvestGeneratedDirectory" `
            @EngineSources `
            -o "$BuildDirectory/poecraft_engine.dll"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ shared engine build failed with exit code $LASTEXITCODE."
        }

        # Keep the opt-in S7 solver benchmark available in compiler-only
        # environments too. CMake builds the same public-API harness through
        # engine/CMakeLists.txt; the fallback has no reusable static library,
        # so it links the engine sources directly.
        $BenchmarkSystemLibraries = @()
        if ($env:OS -eq "Windows_NT") {
            $BenchmarkSystemLibraries += "-lpsapi"
        }
        & $Compiler `
            -std=c++20 -O2 -ffp-contract=off `
            -static-libstdc++ -static-libgcc `
            "-I$Root/engine/include" `
            "-I$Root/engine/src" `
            "-I$HarvestGeneratedDirectory" `
            @EngineSources `
            "$Root/engine/benchmarks/solver_executable_fragment.cpp" `
            "$Root/engine/benchmarks/solver_executable_fragment_engine.cpp" `
            "$Root/engine/benchmarks/solver_benchmark.cpp" `
            @BenchmarkSystemLibraries `
            -o "$BuildDirectory/poecraft_solver_benchmark.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ solver benchmark build failed with exit code $LASTEXITCODE."
        }
        $CompilerDirectory = Split-Path -Parent $Compiler
        $WinPthread = Join-Path $CompilerDirectory "libwinpthread-1.dll"
        if (Test-Path $WinPthread) {
            Copy-Item -Force $WinPthread $BuildDirectory
        }
    }
    else {
        Write-Warning "CMake and a C++20 compiler were not found; C++ engine build was skipped."
    }
}

Write-Host "Build completed."
