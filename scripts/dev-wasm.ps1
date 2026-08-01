[CmdletBinding()]
param(
    [string]$EmsdkRoot,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
. "$PSScriptRoot/engine-build-common.ps1"

if (-not $EmsdkRoot) {
    $EmsdkRoot = if ($env:EMSDK) { $env:EMSDK } else { "C:\emsdk" }
}
$EnvScript = Join-Path $EmsdkRoot "emsdk_env.ps1"
if (-not (Test-Path -LiteralPath $EnvScript)) {
    throw "Emscripten SDK env script not found at $EnvScript."
}

$env:EMSDK_QUIET = "1"
$PreviousPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
. $EnvScript *> $null
$ErrorActionPreference = $PreviousPreference

$Emcmake = Get-Command emcmake -ErrorAction SilentlyContinue |
    Select-Object -First 1
$CMake = Find-PoeCraftCMake
$Ninja = Find-PoeCraftNinja
if (-not $Emcmake -or -not $CMake -or -not $Ninja) {
    throw "Incremental WASM requires emcmake plus the installed CMake and Ninja tools."
}

$BuildDirectory = Join-Path $Root "build/wasm"
if ($Clean -and (Test-Path -LiteralPath $BuildDirectory)) {
    $ResolvedBuild = [System.IO.Path]::GetFullPath($BuildDirectory)
    $ExpectedBuild = [System.IO.Path]::GetFullPath("$Root/build/wasm")
    if ($ResolvedBuild -ne $ExpectedBuild) {
        throw "Refusing to remove unexpected build path: $ResolvedBuild"
    }
    Remove-Item -LiteralPath $ResolvedBuild -Recurse -Force
}

if (-not (Test-Path -LiteralPath "$BuildDirectory/build.ninja")) {
    & $Emcmake.Source $CMake `
        -S "$Root/engine" `
        -B $BuildDirectory `
        -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$Ninja" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
        -DBUILD_TESTING=OFF
    if ($LASTEXITCODE -ne 0) {
        throw "Incremental Emscripten configuration failed with exit code $LASTEXITCODE."
    }
}

& $CMake --build $BuildDirectory --target poecraft_engine_wasm --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Incremental Emscripten build failed with exit code $LASTEXITCODE."
}

$Output = Join-Path $Root "bindings/wasm/dist/poecraft_engine.mjs"
$GeneratedModule = [System.IO.File]::ReadAllText($Output)
$NormalizedModule = $GeneratedModule.Replace("`r`n", "`n").TrimEnd() + "`n"
if ($GeneratedModule -cne $NormalizedModule) {
    [System.IO.File]::WriteAllText(
        $Output,
        $NormalizedModule,
        [System.Text.UTF8Encoding]::new($false))
}

Write-Host "Incremental WASM engine written to $Output"
Write-Host "Use scripts/build-wasm.ps1 for the direct-emcc release fallback."
