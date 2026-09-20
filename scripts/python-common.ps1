# Project Python resolution. Emscripten's SDK interpreter remains SDK-owned.
function Get-PoeCraftPython {
    $Prefix = @()
    if ($env:POECRAFT_PYTHON) {
        $Command = $env:POECRAFT_PYTHON
    }
    elseif (Get-Command py -ErrorAction SilentlyContinue) {
        $Command = (Get-Command py).Source
        $Prefix = @("-3")
    }
    elseif (Get-Command python -ErrorAction SilentlyContinue) {
        $Command = (Get-Command python).Source
    }
    else {
        throw "Python was not found. Set POECRAFT_PYTHON to a Python 3.11+ executable."
    }
    $Resolved = & $Command @Prefix -c "import sys; assert sys.version_info >= (3, 11), 'Python 3.11+ required'; print(sys.executable)"
    if ($LASTEXITCODE -ne 0 -or -not $Resolved -or
        -not (Test-Path -LiteralPath $Resolved -PathType Leaf)) {
        throw "Could not resolve the selected project Python: $Command"
    }
    # Bind nested project scripts and pip to the same executable, even if an
    # SDK activation subsequently changes PATH or another Python is installed.
    $env:POECRAFT_PYTHON = [System.IO.Path]::GetFullPath($Resolved)
    return @{ Command = $env:POECRAFT_PYTHON; Prefix = @() }
}
