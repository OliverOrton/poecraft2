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

$Exported = @(
    "_pcw_abi_version", "_pcw_response_data", "_pcw_response_size",
    "_pcw_response_clear",
    "_pcw_data_open", "_pcw_data_summary", "_pcw_data_bases",
    "_pcw_bestiary_presentation", "_pcw_bestiary_apply",
    "_pcw_bestiary_calculate",
    "_pcw_data_close",
    "_pcw_session_open", "_pcw_session_close", "_pcw_session_mod_count",
    "_pcw_session_mod_info", "_pcw_context_open", "_pcw_context_close",
    "_pcw_item_create", "_pcw_item_clone", "_pcw_item_close", "_pcw_item_info",
    "_pcw_item_add_mod", "_pcw_item_remove_mod", "_pcw_item_set_mod_fractured",
    "_pcw_item_export", "_pcw_item_import",
    "_pcw_apply", "_pcw_run_batch", "_pcw_debug_pool",
    "_pcw_strategy_compile", "_pcw_strategy_close", "_pcw_strategy_evaluate",
    "_pcw_strategy_eval_begin", "_pcw_strategy_eval_step",
    "_pcw_strategy_eval_finish", "_pcw_strategy_eval_close",
    "_pcw_live_handle_count", "_pcw_memory_stats",
    "_pcw_economy_open", "_pcw_economy_close",
    "_pcw_simulator_open", "_pcw_simulator_close",
    "_pcw_simulator_run_chunk", "_pcw_simulator_result",
    "_pcw_solver_open", "_pcw_solver_close", "_pcw_solver_actions",
    "_pcw_solver_calc", "_pcw_solver_solve", "_pcw_solver_state_value",
    "_pcw_solver_project", "_pcw_solver_compile",
    "_pcw_solver_compile_transfer", "_pcw_solver_log",
    "_pcw_solver_telemetry",
    "_pcw_solver_solve_begin", "_pcw_solver_solve_step",
    "_pcw_solver_solve_finish", "_pcw_solver_solve_abandon",
    "_malloc", "_free"
) -join ","

$RuntimeMethods = @("ccall", "cwrap", "UTF8ToString", "HEAPU8") -join ","

$EmccArgs = @(
    "-std=c++20", "-O3", "-ffp-contract=off", "-fexceptions",
    "-I$Root/engine/include", "-I$Root/engine/src", "-I$GeneratedDirectory"
)
if ($Diagnostics) {
    $EmccArgs += @(
        "-sASSERTIONS=2",
        "-sSAFE_HEAP=1",
        "-sSTACK_OVERFLOW_CHECK=2"
    )
}
$EmccArgs += $EngineSources
$EmccArgs += $Facade
$EmccArgs += @(
    "-fexceptions",
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
