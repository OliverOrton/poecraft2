# Supplied UNRUN: review environment has no PowerShell.
# Run with the workflow's actual shell and its selected Python executable.
# Read-only probe; no build, fetch, artifact regeneration or repository mutation.
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$LockFile,
    [string]$Python = $env:POECRAFT_PYTHON
)
$ErrorActionPreference = 'Stop'
if (-not $Python -or -not (Test-Path -LiteralPath $Python -PathType Leaf)) {
    throw 'Pass the exact selected Python executable using -Python or POECRAFT_PYTHON.'
}
$LockFile = (Resolve-Path -LiteralPath $LockFile).Path
$LiteralCode = @'
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    value = json.load(f)["runtime_artifact"]["generated_at_utc"]
if not isinstance(value, str) or not value:
    raise ValueError("expected a nonempty literal timestamp string")
print(value)
'@
$Literal = & $Python -c $LiteralCode $LockFile
if ($LASTEXITCODE -ne 0) { throw 'Literal JSON extraction failed.' }
$Parsed = (Get-Content -LiteralPath $LockFile -Raw | ConvertFrom-Json).runtime_artifact.generated_at_utc
if ($null -eq $Parsed) { throw 'PowerShell produced no timestamp.' }
$TypeName = $Parsed.GetType().FullName
$Version = $PSVersionTable.PSVersion.ToString()
$ProbeCode = @'
import json, sys
literal, parsed_type, shell, received = sys.argv[1:]
print(json.dumps({"powershell_version": shell, "parsed_type": parsed_type,
    "literal_lock_string": literal, "string_array_bound_python_argument": received,
    "literal_preserved": literal == received}, indent=2))
raise SystemExit(0 if literal == received else 1)
'@
function Invoke-StringArrayProbe {
    param([string[]]$Arguments)
    # This deliberately exercises the same string-array parameter boundary.
    & $Python -c $ProbeCode @Arguments
}
Invoke-StringArrayProbe -Arguments @($Literal, $TypeName, $Version, $Parsed)
exit $LASTEXITCODE
