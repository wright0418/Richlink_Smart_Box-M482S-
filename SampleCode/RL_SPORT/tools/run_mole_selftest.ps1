param(
    [string]$Com = "COM3",
    [int]$Baud = 115200,
    [int]$Count = 1,
    [switch]$SkipBuild,
    [switch]$SkipLoad,
    [switch]$NoTrace,
    [int]$ManualHitSeconds = 6,
    [string]$PythonExe = ""
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vscode = Join-Path $root 'VSCode'
$csolution = Join-Path $vscode 'RL_SPORT.csolution.yml'
$firmware = Join-Path $vscode 'out\RL_SPORT\ARMCLANG\release\RL_SPORT.axf'
$pyScript = Join-Path $PSScriptRoot 'mole_ble_selftest.py'

if (-not (Test-Path $pyScript)) {
    throw "Self-test script not found: $pyScript"
}

function Resolve-PythonExe {
    param([string]$Preferred)

    if (-not [string]::IsNullOrWhiteSpace($Preferred)) {
        if (Test-Path $Preferred) { return $Preferred }
        throw "Specified PythonExe not found: $Preferred"
    }

    $repoVenv = Join-Path $root '.venv\Scripts\python.exe'
    if (Test-Path $repoVenv) {
        return $repoVenv
    }

    $candidates = @(
        "python",
        "py -3",
        "py"
    )

    foreach ($cand in $candidates) {
        try {
            if ($cand -eq "python") {
                & python --version *> $null
                if ($LASTEXITCODE -eq 0) { return "python" }
            }
            elseif ($cand -eq "py -3") {
                & py -3 --version *> $null
                if ($LASTEXITCODE -eq 0) { return "py -3" }
            }
            else {
                & py --version *> $null
                if ($LASTEXITCODE -eq 0) { return "py" }
            }
        }
        catch {
            # try next candidate
        }
    }

    throw "No usable Python interpreter found. Install Python 3 and ensure 'python' or 'py' is available in PATH."
}

$pythonCmd = Resolve-PythonExe -Preferred $PythonExe

Push-Location $vscode
try {
    if (-not $SkipBuild) {
        Write-Host "[SELFTEST] Build release firmware..."
        & cbuild $csolution -c RL_SPORT.release+ARMCLANG
        if ($LASTEXITCODE -ne 0) { throw "cbuild failed: $LASTEXITCODE" }
    }

    if (-not $SkipLoad) {
        if (-not (Test-Path $firmware)) {
            throw "Firmware not found: $firmware"
        }
        Write-Host "[SELFTEST] Load firmware..."
        & pyocd load --probe cmsisdap: --target m487jidae $firmware
        if ($LASTEXITCODE -ne 0) { throw "pyocd load failed: $LASTEXITCODE" }

        Write-Host "[SELFTEST] Reset target..."
        & pyocd reset --probe cmsisdap: --target m487jidae
        if ($LASTEXITCODE -ne 0) { throw "pyocd reset failed: $LASTEXITCODE" }
    }
}
finally {
    Pop-Location
}

Write-Host "[SELFTEST] Run BLE + COM cross verification..."
$extraArgs = @()
if ($NoTrace) {
    $extraArgs += '--no-trace'
}

if ($pythonCmd -eq "python") {
    & python $pyScript --com $Com --baud $Baud --count $Count --manual-hit-seconds $ManualHitSeconds @extraArgs
}
elseif ($pythonCmd -eq "py -3") {
    & py -3 $pyScript --com $Com --baud $Baud --count $Count --manual-hit-seconds $ManualHitSeconds @extraArgs
}
elseif ($pythonCmd -eq "py") {
    & py $pyScript --com $Com --baud $Baud --count $Count --manual-hit-seconds $ManualHitSeconds @extraArgs
}
else {
    & $pythonCmd $pyScript --com $Com --baud $Baud --count $Count --manual-hit-seconds $ManualHitSeconds @extraArgs
}
if ($LASTEXITCODE -ne 0) {
    throw "mole_ble_selftest.py failed: $LASTEXITCODE"
}

Write-Host "[SELFTEST] All done."
