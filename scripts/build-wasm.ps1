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
