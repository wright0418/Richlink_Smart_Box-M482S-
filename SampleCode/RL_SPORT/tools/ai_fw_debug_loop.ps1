param(
    [string]$PortName,
    [int]$BaudRate = 115200,
    [switch]$SkipBuild,
    [switch]$SkipLoad,
    [int]$StartupCaptureMs = 0,
    [int]$CommandCaptureMs = 1500,
    [string[]]$Commands = @(
        'AT+TEST=INFO'
    )
)

$ErrorActionPreference = 'Stop'

function Assert-LastExitCode {
    param(
        [string]$StepName
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$StepName failed with exit code $LASTEXITCODE"
    }
}

function Get-ScriptPaths {
    $rlSportRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
    $vscodeRoot = Join-Path $rlSportRoot 'VSCode'
    $csolution = Join-Path $vscodeRoot 'RL_SPORT.csolution.yml'
    $cbuildIdx = Join-Path $vscodeRoot 'RL_SPORT.cbuild-idx.yml'

    return [PSCustomObject]@{
        RlSportRoot = $rlSportRoot
        RepoRoot    = $repoRoot
        VsCodeRoot  = $vscodeRoot
        Csolution   = $csolution
        CbuildIdx   = $cbuildIdx
    }
}

function Get-CbuildRunPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CbuildIdxPath,
        [Parameter(Mandatory = $true)]
        [string]$VsCodeRoot
    )

    if (-not (Test-Path $CbuildIdxPath)) {
        throw "cbuild index file not found: $CbuildIdxPath"
    }

    $raw = Get-Content -Raw -Path $CbuildIdxPath
    $match = [regex]::Match($raw, 'cbuild-run:\s*(.+)')
    if (-not $match.Success) {
        throw "Unable to find cbuild-run entry in $CbuildIdxPath"
    }

    $relativePath = $match.Groups[1].Value.Trim()
    $fullPath = Join-Path $VsCodeRoot $relativePath
    if (-not (Test-Path $fullPath)) {
        throw "cbuild-run file does not exist yet: $fullPath"
    }

    return $fullPath
}

function Get-NuLinkSerialPort {
    $ports = @(Get-CimInstance Win32_SerialPort | Sort-Object DeviceID)
    if (-not $ports -or $ports.Count -eq 0) {
        throw 'No serial ports detected on this machine.'
    }

    $preferred = @(
        $ports | Where-Object {
            ($_.Name -match 'Nu.?Link') -or
            ($_.Name -match 'Virtual COM Port') -or
            ($_.Description -match 'Nu.?Link') -or
            ($_.Description -match 'Virtual COM Port') -or
            ($_.Caption -match 'Nu.?Link') -or
            ($_.Caption -match 'Virtual COM Port')
        }
    )

    if ($preferred.Count -gt 0) {
        return $preferred[0].DeviceID
    }

    $portsSummary = ($ports | ForEach-Object { "{0} ({1})" -f $_.DeviceID, $_.Name }) -join '; '
    throw "NuLink2 Virtual COM port not found automatically. Available ports: $portsSummary"
}

function New-SerialPort {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Device,
        [Parameter(Mandatory = $true)]
        [int]$Baud
    )

    $port = New-Object System.IO.Ports.SerialPort $Device, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
    $port.Handshake = [System.IO.Ports.Handshake]::None
    $port.NewLine = "`r`n"
    $port.ReadTimeout = 200
    $port.WriteTimeout = 1000
    $port.Encoding = [System.Text.Encoding]::ASCII
    return $port
}

function Read-SerialWindow {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port,
        [Parameter(Mandatory = $true)]
        [int]$DurationMs,
        [Parameter(Mandatory = $true)]
        [System.Text.StringBuilder]$LogBuffer,
        [string]$Prefix = '',
        [string]$StopPattern
    )

    $deadline = [Environment]::TickCount64 + $DurationMs
    $captured = ''

    while ([Environment]::TickCount64 -lt $deadline) {
        try {
            $chunk = $Port.ReadExisting()
        }
        catch {
            if (-not [string]::IsNullOrEmpty($captured)) {
                break
            }
            throw
        }

        if (-not [string]::IsNullOrEmpty($chunk)) {
            $captured += $chunk
            [void]$LogBuffer.Append($chunk)
            if ([string]::IsNullOrEmpty($Prefix)) {
                Write-Host $chunk -NoNewline
            }
            else {
                Write-Host "$Prefix$chunk" -NoNewline
            }

            if (-not [string]::IsNullOrEmpty($StopPattern) -and ($captured -match $StopPattern)) {
                break
            }
        }
    }

    return $captured
}

function Send-SerialCommand {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port,
        [Parameter(Mandatory = $true)]
        [string]$Command,
        [Parameter(Mandatory = $true)]
        [System.Text.StringBuilder]$LogBuffer,
        [Parameter(Mandatory = $true)]
        [int]$CaptureMs,
        [string]$StopPattern = '\+TEST:'
    )

    Write-Host "`n[UART TX] $Command"
    [void]$LogBuffer.AppendLine("[UART TX] $Command")
    $Port.Write("$Command`r`n")
    return Read-SerialWindow -Port $Port -DurationMs $CaptureMs -LogBuffer $LogBuffer -Prefix '[UART RX] ' -StopPattern $StopPattern
}

$paths = Get-ScriptPaths
$logDir = Join-Path $env:TEMP 'rl_sport_ai_fw_debug'
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $logDir ("rl_sport_ai_fw_debug_{0}.log" -f $timestamp)
$logBuffer = New-Object System.Text.StringBuilder

Write-Host "[AI-FW] RL_SPORT root  : $($paths.RlSportRoot)"
Write-Host "[AI-FW] csolution     : $($paths.Csolution)"
Write-Host "[AI-FW] log output    : $logPath"

if (-not $SkipBuild) {
    Write-Host '[AI-FW] Step 1/4: build debug firmware'
    & cbuild $paths.Csolution --context-set --packs
    Assert-LastExitCode 'cbuild'
}

$cbuildRun = Get-CbuildRunPath -CbuildIdxPath $paths.CbuildIdx -VsCodeRoot $paths.VsCodeRoot
Write-Host "[AI-FW] cbuild-run    : $cbuildRun"

if (-not $SkipLoad) {
    Write-Host '[AI-FW] Step 2/4: load firmware to target'
    & pyocd load --probe 'cmsisdap:' --cbuild-run $cbuildRun
    Assert-LastExitCode 'pyocd load'
}

if (-not $PortName) {
    $PortName = Get-NuLinkSerialPort
}

Write-Host "[AI-FW] Step 3/4: open serial port $PortName @ $BaudRate"
$serial = New-SerialPort -Device $PortName -Baud $BaudRate

$serialOpened = $false
try {
    $serial.Open()
    $serialOpened = $true
    $serial.DiscardInBuffer()
    $serial.DiscardOutBuffer()

    $startupOutput = ''
    if ($StartupCaptureMs -gt 0) {
        Write-Host "[AI-FW] Step 4/4: capture startup log for ${StartupCaptureMs}ms"
        $startupOutput = Read-SerialWindow -Port $serial -DurationMs $StartupCaptureMs -LogBuffer $logBuffer
    }
    else {
        Write-Host '[AI-FW] Step 4/4: skip startup wait and issue UART commands immediately'
    }

    $responses = @()
    foreach ($command in $Commands) {
        if ([string]::IsNullOrWhiteSpace($command)) {
            continue
        }
        $resp = Send-SerialCommand -Port $serial -Command $command -LogBuffer $logBuffer -CaptureMs $CommandCaptureMs
        $responses += $resp
    }

    $allOutput = ($startupOutput + ($responses -join "`n"))
    $commandOutput = ($responses -join "`n")
    $sawStartup = ($startupOutput -match 'System Up' -or $startupOutput -match '\[DEBUG\]')
    $sawCommandResponse = ($commandOutput -match '\+TEST:' -or $commandOutput -match '(^|\r?\n)OK(\r?\n|$)')

    [System.IO.File]::WriteAllText($logPath, $logBuffer.ToString(), [System.Text.Encoding]::UTF8)

    if ($sawCommandResponse) {
        Write-Host "`n[AI-FW] Closed-loop UART debug succeeded. Log saved to: $logPath"
        exit 0
    }

    if ($sawStartup) {
        throw "MCU UART output was detected, but the scripted AT command round-trip did not complete before the target stopped responding. Log: $logPath"
    }

    throw "Closed-loop run finished but no expected MCU UART output was detected. Log: $logPath"
}
finally {
    if ($serialOpened -and $serial.IsOpen) {
        $serial.Close()
    }
    $serial.Dispose()
}