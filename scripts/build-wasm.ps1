[CmdletBinding()]
param(
    # Root of an activated Emscripten SDK. Defaults to $env:EMSDK, then C:\emsdk.
    [string]$EmsdkRoot
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

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

$EngineSources = Get-ChildItem -Path "$Root/engine/src" -Filter *.cpp |
    ForEach-Object { $_.FullName }
$Facade = Join-Path $Root "bindings/wasm/wasm_api.cpp"

$Exported = @(
    "_pcw_abi_version", "_pcw_data_open", "_pcw_data_summary", "_pcw_data_bases",
    "_pcw_data_close",
    "_pcw_session_open", "_pcw_session_close", "_pcw_session_mod_count",
    "_pcw_session_mod_info", "_pcw_context_open", "_pcw_context_close",
    "_pcw_item_create", "_pcw_item_clone", "_pcw_item_close", "_pcw_item_info",
    "_pcw_item_add_mod", "_pcw_apply", "_pcw_run_batch", "_pcw_debug_pool",
    "_malloc", "_free"
) -join ","

$RuntimeMethods = @("ccall", "cwrap", "UTF8ToString", "HEAPU8") -join ","

$EmccArgs = @(
    "-std=c++20", "-O2", "-fexceptions",
    "-I$Root/engine/include", "-I$Root/engine/src"
)
$EmccArgs += $EngineSources
$EmccArgs += $Facade
$EmccArgs += @(
    "-fexceptions",
    "-sMODULARIZE=1",
    "-sEXPORT_ES6=1",
    "-sEXPORT_NAME=createPoecraftEngine",
    "-sENVIRONMENT=web,worker,node",
    "-sALLOW_MEMORY_GROWTH=1",
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

Write-Host "WASM engine written to $Output"
