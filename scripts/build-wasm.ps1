[CmdletBinding()]
param(
    # Root of an activated Emscripten SDK. Defaults to $env:EMSDK, then C:\emsdk.
    [string]$EmsdkRoot,
    [switch]$Diagnostics
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/engine-build-common.ps1"

if (-not $EmsdkRoot) {
    $EmsdkRoot = if ($env:EMSDK) { $env:EMSDK } else { "C:\emsdk" }
}
$EnvScript = Join-Path $EmsdkRoot "emsdk_env.ps1"
if (-not (Test-Path -LiteralPath $EnvScript)) {
    throw "Emscripten SDK env script not found at $EnvScript. Install/activate emsdk or pass -EmsdkRoot."
}
# Bring emcc and EMSDK_PYTHON/EMSDK_NODE into this session. The env script and
# emcc emit INFO lines on stderr; relax the error preference so those benign
# native-stderr writes are not treated as terminating.
$env:EMSDK_QUIET = "1"
$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
. $EnvScript *> $null
$ErrorActionPreference = $PreviousPreference

$Emcc = Get-Command emcc -ErrorAction SilentlyContinue
if (-not $Emcc) {
    throw "emcc was not found on PATH after sourcing $EnvScript."
}

$OutputDirectory = Join-Path $Root "bindings/wasm/dist"
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$Output = Join-Path $OutputDirectory "poecraft_engine.mjs"
$GeneratedDirectory = Join-Path $Root "build/wasm/generated"
$GeneratedHeader = Join-Path $GeneratedDirectory "harvest_crafts.generated.hpp"
py -3 "$Root/scripts/generate-harvest-crafts.py" `
    --recipes "$Root/fixtures/economy/harvest-recipes-v1.json" `
    --output $GeneratedHeader
if ($LASTEXITCODE -ne 0) {
    throw "Harvest craft allowlist generation failed with exit code $LASTEXITCODE."
}

$EngineSources = Get-PoeCraftEngineSources -Root $Root
$Facade = Join-Path $Root "bindings/wasm/wasm_api.cpp"

# Emscripten 6.0.0's LTO coroutine lowering produces invalid SSA for the
# retained finalization continuation in this translation unit. Keep the hot
# engine kernels under full LTO, but lower this orchestration-only source to a
# regular optimized WebAssembly object before the final link.
$FinishSource = [System.IO.Path]::GetFullPath(
    (Join-Path $Root "engine/src/solver_solve_finish.cpp"))
$LtoEngineSources = @(
    $EngineSources | Where-Object { $_ -ne $FinishSource }
)
$ObjectDirectory = Join-Path $Root "build/wasm/objects"
$FinishObject = Join-Path $ObjectDirectory "solver_solve_finish.o"
New-Item -ItemType Directory -Force -Path $ObjectDirectory | Out-Null

$ExportManifest = Join-Path $Root "bindings/wasm/wasm-exports.txt"
$ExportEntries = @(
    Get-Content -LiteralPath $ExportManifest |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith("#") }
)
if ($ExportEntries.Count -eq 0 -or
        ($ExportEntries | Select-Object -Unique).Count -ne $ExportEntries.Count -or
        @($ExportEntries | Where-Object { $_ -notmatch '^_[A-Za-z0-9_]+$' }).Count -ne 0) {
    throw "Invalid or duplicate WASM export in $ExportManifest."
}
$Exported = $ExportEntries -join ","

$RuntimeMethods = @("ccall", "cwrap", "UTF8ToString", "HEAPU8") -join ","

$SharedCompileArgs = @(
    "-msimd128", "-ffp-contract=off",
    # Use native WebAssembly exception handling. The legacy JS exception
    # lowering inhibits optimization across every potentially throwing engine
    # call, which is material for long exact solver qualifications.
    "-fwasm-exceptions",
    "-I$Root/engine/include", "-I$Root/engine/src", "-I$GeneratedDirectory"
)
if ($Diagnostics) {
    $SharedCompileArgs += @(
        "-sASSERTIONS=2",
        "-sSAFE_HEAP=1",
        "-sSTACK_OVERFLOW_CHECK=2"
    )
}
$FinishCompileArgs = @("-std=c++20", "-O3") + $SharedCompileArgs

Write-Host "Compiling cooperative finalization WASM object..."
$ErrorActionPreference = "Continue"
& $Emcc.Source @FinishCompileArgs "-c" $FinishSource "-o" $FinishObject
$FinishExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousPreference
if ($FinishExit -ne 0) {
    throw "cooperative finalization WASM compile failed with exit code $FinishExit."
}

$EmccArgs = @(
    "-std=c++20", "-O3", "-flto"
)
$EmccArgs += $SharedCompileArgs
$EmccArgs += $LtoEngineSources
$EmccArgs += $FinishObject
$EmccArgs += $Facade
$EmccArgs += @(
    "-fwasm-exceptions",
    "-sMODULARIZE=1",
    "-sEXPORT_ES6=1",
    "-sEXPORT_NAME=createPoecraftEngine",
    "-sENVIRONMENT=web,worker,node",
    "-sALLOW_MEMORY_GROWTH=1",
    "-sINITIAL_MEMORY=134217728",
    "-sMAXIMUM_MEMORY=4GB",
    # Large compiled policy conditions are constructed and parsed recursively;
    # the advanced S7 corpus exceeds Emscripten's 4 MiB default-safe stack.
    "-sSTACK_SIZE=67108864",
    "-sEXIT_RUNTIME=0",
    "-sEXPORTED_FUNCTIONS=$Exported",
    "-sEXPORTED_RUNTIME_METHODS=$RuntimeMethods",
    "-o", $Output
)

Write-Host "Compiling poecraft engine to WebAssembly..."
# emcc writes progress/INFO to stderr; judge success by exit code, not stderr.
$ErrorActionPreference = "Continue"
& $Emcc.Source @EmccArgs
$EmccExit = $LASTEXITCODE
$ErrorActionPreference = $PreviousPreference
if ($EmccExit -ne 0) {
    throw "emcc build failed with exit code $EmccExit."
}

# Emscripten emits the minified ES module with CRLF and trailing whitespace.
# Normalize the committed wrapper so rebuilds stay diff-check clean.
$GeneratedModule = [System.IO.File]::ReadAllText($Output)
$GeneratedModule = $GeneratedModule.Replace("`r`n", "`n").TrimEnd() + "`n"
[System.IO.File]::WriteAllText(
    $Output,
    $GeneratedModule,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "WASM engine written to $Output"
