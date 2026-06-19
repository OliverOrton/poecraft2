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

        $EngineSources = Get-ChildItem -Path "$Root/engine/src" -Filter *.cpp |
            ForEach-Object { $_.FullName }
        $TestSources = @(
            "$Root/engine/tests/test_main.cpp",
            "$Root/engine/tests/test_bitset.cpp",
            "$Root/engine/tests/test_rng.cpp",
            "$Root/engine/tests/test_item_state.cpp",
            "$Root/engine/tests/test_blocking.cpp",
            "$Root/engine/tests/test_data_loader.cpp",
            "$Root/engine/tests/test_session_builder.cpp",
            "$Root/engine/tests/test_actions.cpp",
            "$Root/engine/tests/test_simulator.cpp"
        )
        # -static-libstdc++/-static-libgcc avoid a ld.bfd crash (exit 116) seen
        # in MSYS2 ucrt64 binutils when linking the shared C++ runtime against
        # exception-bearing COMDAT sections. They also make the test binary
        # self-contained. CMake/MSVC builds do not need this.
        & $Compiler.Source `
            -std=c++20 -O2 `
            -static-libstdc++ -static-libgcc `
            "-I$Root/engine/include" `
            @EngineSources `
            @TestSources `
            -o "$BuildDirectory/poecraft_engine_tests.exe"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ engine test build failed with exit code $LASTEXITCODE."
        }

        & $Compiler.Source `
            -std=c++20 -O2 -shared `
            -static-libstdc++ -static-libgcc `
            "-Wl,--export-all-symbols" `
            "-I$Root/engine/include" `
            @EngineSources `
            -o "$BuildDirectory/poecraft_engine.dll"
        if ($LASTEXITCODE -ne 0) {
            throw "C++ shared engine build failed with exit code $LASTEXITCODE."
        }
        $CompilerDirectory = Split-Path -Parent $Compiler.Source
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
