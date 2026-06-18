[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

$Python = if ($env:POECRAFT_PYTHON) {
    @{
        Command = $env:POECRAFT_PYTHON
        Prefix = @()
    }
}
elseif (Get-Command py -ErrorAction SilentlyContinue) {
    @{
        Command = (Get-Command py).Source
        Prefix = @("-3")
    }
}
else {
    @{
        Command = (Get-Command python -ErrorAction Stop).Source
        Prefix = @()
    }
}

$RuffAvailable = $false
& $Python.Command @($Python.Prefix) -c "import importlib.util, sys; sys.exit(0 if importlib.util.find_spec('ruff') else 1)"
if ($LASTEXITCODE -eq 0) {
    $RuffAvailable = $true
}

if ($RuffAvailable) {
    & $Python.Command @($Python.Prefix) -m ruff format "$Root/tools/ingest"
    if ($LASTEXITCODE -ne 0) {
        throw "ruff format failed with exit code $LASTEXITCODE."
    }
    & $Python.Command @($Python.Prefix) -m ruff check --fix "$Root/tools/ingest"
    if ($LASTEXITCODE -ne 0) {
        throw "ruff check failed with exit code $LASTEXITCODE."
    }
}
else {
    & $Python.Command @($Python.Prefix) -m compileall -q "$Root/tools/ingest/poecraft_ingest"
    if ($LASTEXITCODE -ne 0) {
        throw "Python compile check failed with exit code $LASTEXITCODE."
    }
    Write-Warning "ruff is not installed; formatting was limited to Python syntax validation."
}

git -C $Root diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check failed with exit code $LASTEXITCODE."
}
