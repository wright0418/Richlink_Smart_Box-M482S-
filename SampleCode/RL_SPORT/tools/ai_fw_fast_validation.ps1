param(
    [string]$PortName,
    [int]$BaudRate = 115200,
    [switch]$SkipBuild,
    [switch]$SkipLoad,
    [switch]$SkipLongTests,
    [switch]$IncludeAllSmoke,
    [int]$SuiteTimeoutMs = 25000,
    [int]$DefaultFirstByteTimeoutMs = 250,
    [int]$DefaultIdleTimeoutMs = 180
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
    $vscodeRoot = Join-Path $rlSportRoot 'VSCode'
    $csolution = Join-Path $vscodeRoot 'RL_SPORT.csolution.yml'
    $cbuildIdx = Join-Path $vscodeRoot 'RL_SPORT.cbuild-idx.yml'

    return [PSCustomObject]@{
        RlSportRoot = $rlSportRoot
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

function Wait-NuLinkSerialPort {
    param(
        [int]$TimeoutMs = 3000,
        [int]$PollMs = 100
    )

    $start = [Environment]::TickCount64
    while (([Environment]::TickCount64 - $start) -lt $TimeoutMs) {
        try {
            return Get-NuLinkSerialPort
        }
        catch {
            [System.Threading.Thread]::Sleep([int]$PollMs)
        }
    }

    return Get-NuLinkSerialPort
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
    $port.ReadTimeout = 100
    $port.WriteTimeout = 1000
    $port.Encoding = [System.Text.Encoding]::ASCII
    return $port
}

function Open-PortIfNeeded {
    param(
        [ref]$PortRef,
        [Parameter(Mandatory = $true)]
        [string]$PortName,
        [Parameter(Mandatory = $true)]
        [int]$BaudRate,
        [int]$MaxOpenAttempts = 3,
        [int]$OpenRetryDelayMs = 150
    )

    for ($attempt = 1; $attempt -le $MaxOpenAttempts; $attempt++) {
        try {
            if ($null -eq $PortRef.Value) {
                $PortRef.Value = New-SerialPort -Device $PortName -Baud $BaudRate
            }

            if (-not $PortRef.Value.IsOpen) {
                $PortRef.Value.Open()
            }

            return
        }
        catch {
            Close-Port -PortRef $PortRef
            if ($attempt -ge $MaxOpenAttempts) {
                throw
            }
            [System.Threading.Thread]::Sleep([int]$OpenRetryDelayMs)
        }
    }
}

function Close-Port {
    param(
        [ref]$PortRef
    )

    if ($null -ne $PortRef.Value) {
        if ($PortRef.Value.IsOpen) {
            $PortRef.Value.Close()
        }
        $PortRef.Value.Dispose()
        $PortRef.Value = $null
    }
}

function Reset-PortBuffers {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port
    )

    try {
        [void]$Port.ReadExisting()
    }
    catch {
    }
    $Port.DiscardInBuffer()
    $Port.DiscardOutBuffer()
}

function Reset-TargetByLoad {
    param(
        [Parameter(Mandatory = $true)]
        [ref]$PortRef,
        [Parameter(Mandatory = $true)]
        [string]$CbuildRun,
        [ref]$PortNameRef,
        [int]$SettleMs = 500
    )

    Close-Port -PortRef $PortRef
    Write-Host "[FAST] Recovery: reload firmware to reset MCU"
    & pyocd load --probe 'cmsisdap:' --cbuild-run $CbuildRun
    Assert-LastExitCode 'pyocd load (recovery)'
    [System.Threading.Thread]::Sleep([int]$SettleMs)
    $PortNameRef.Value = Wait-NuLinkSerialPort -TimeoutMs 3000 -PollMs 100
}

function Read-UntilMatch {
    param(
        [Parameter(Mandatory = $true)]
        [System.IO.Ports.SerialPort]$Port,
        [Parameter(Mandatory = $true)]
        [int]$TimeoutMs,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedPattern,
        [Parameter(Mandatory = $true)]
        [System.Text.StringBuilder]$LogBuffer,
        [int]$FirstByteTimeoutMs = 250,
        [int]$IdleTimeoutMs = 180
    )

    $startTick = [Environment]::TickCount64
    $deadline = [Environment]::TickCount64 + $TimeoutMs
    $lastDataTick = $startTick
    $captured = ''
    $firstByteTimedOut = $false
    $idleTimedOut = $false
    $sawExplicitFailure = $false
    $readExceptionMessage = ''

    while ([Environment]::TickCount64 -lt $deadline) {
        $now = [Environment]::TickCount64
        try {
            $chunk = $Port.ReadExisting()
        }
        catch {
            $readExceptionMessage = $_.Exception.Message
            break
        }

        if (-not [string]::IsNullOrEmpty($chunk)) {
            $captured += $chunk
            [void]$LogBuffer.Append($chunk)
            Write-Host $chunk -NoNewline
            $lastDataTick = $now

            if ($captured -match $ExpectedPattern) {
                break
            }
            if ($captured -match '\+TEST:[^\r\n]*,FAIL' -or $captured -match '(^|\r?\n)ERROR(\r?\n|$)') {
                $sawExplicitFailure = $true
                break
            }
        }
        else {
            if ([string]::IsNullOrEmpty($captured)) {
                if (($now - $startTick) -ge $FirstByteTimeoutMs) {
                    $firstByteTimedOut = $true
                    break
                }
            }
            elseif (($now - $lastDataTick) -ge $IdleTimeoutMs) {
                $idleTimedOut = $true
                break
            }

            [System.Threading.Thread]::Sleep(10)
        }
    }

    return [PSCustomObject]@{
        Output               = $captured
        HasExpected          = ($captured -match $ExpectedPattern)
        SawExplicitFailure   = $sawExplicitFailure
        FirstByteTimedOut    = $firstByteTimedOut
        IdleTimedOut         = $idleTimedOut
        ReadExceptionMessage = $readExceptionMessage
    }
}

function Invoke-FastTest {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Test,
        [Parameter(Mandatory = $true)]
        [ref]$PortRef,
        [Parameter(Mandatory = $true)]
        [string]$PortName,
        [Parameter(Mandatory = $true)]
        [int]$BaudRate,
        [Parameter(Mandatory = $true)]
        [System.Text.StringBuilder]$LogBuffer,
        [Parameter(Mandatory = $true)]
        [int]$DefaultFirstByteTimeoutMs,
        [Parameter(Mandatory = $true)]
        [int]$DefaultIdleTimeoutMs,
        [Parameter(Mandatory = $true)]
        [string]$CbuildRun,
        [Parameter(Mandatory = $true)]
        [ref]$PortNameRef,
        [int]$RecoverySettleMs = 300,
        [int]$MaxRecoveryAttempts = 1
    )

    $firstByteTimeoutMs = if ($Test.ContainsKey('FirstByteTimeoutMs')) { [int]$Test.FirstByteTimeoutMs } else { $DefaultFirstByteTimeoutMs }
    $idleTimeoutMs = if ($Test.ContainsKey('IdleTimeoutMs')) { [int]$Test.IdleTimeoutMs } else { $DefaultIdleTimeoutMs }

    $attempt = 0
    $recovered = $false
    $response = ''
    $pass = $false
    $recoveryReason = ''

    while ($true) {
        $attempt++
        $recoveryReason = ''

        try {
            Open-PortIfNeeded -PortRef $PortRef -PortName $PortNameRef.Value -BaudRate $BaudRate
            Reset-PortBuffers -Port $PortRef.Value

            Write-Host "`n[FAST TX] $($Test.Command)"
            [void]$LogBuffer.AppendLine("[FAST TX] $($Test.Command)")
            $PortRef.Value.Write("$($Test.Command)`r`n")

            $readResult = Read-UntilMatch -Port $PortRef.Value -TimeoutMs $Test.TimeoutMs -ExpectedPattern $Test.Expect -LogBuffer $LogBuffer -FirstByteTimeoutMs $firstByteTimeoutMs -IdleTimeoutMs $idleTimeoutMs
            $response = $readResult.Output
            $pass = $readResult.HasExpected

            if (-not $pass -and -not $readResult.SawExplicitFailure) {
                if (-not [string]::IsNullOrEmpty($readResult.ReadExceptionMessage)) {
                    $recoveryReason = $readResult.ReadExceptionMessage
                }
                elseif ($readResult.FirstByteTimedOut -or [string]::IsNullOrWhiteSpace($response)) {
                    $recoveryReason = 'no UART response'
                }
            }
        }
        catch {
            $response = ''
            $pass = $false
            $recoveryReason = $_.Exception.Message
        }

        if ([string]::IsNullOrEmpty($recoveryReason) -or $attempt -gt $MaxRecoveryAttempts) {
            break
        }

        $recovered = $true
        Write-Host "[FAST] $($Test.Name) recovery attempt ${attempt}/${MaxRecoveryAttempts}: $recoveryReason"
        Reset-TargetByLoad -PortRef $PortRef -CbuildRun $CbuildRun -PortNameRef $PortNameRef -SettleMs $RecoverySettleMs
    }

    $tail = (($response -replace "`r", ' ') -replace "`n", ' ').Trim()

    Write-Host "`n[FAST RESULT] $($Test.Name) => $(if ($pass) { 'PASS' } else { 'FAIL' })"

    if ($Test.DisconnectAfterMs -gt 0) {
        Close-Port -PortRef $PortRef
        [System.Threading.Thread]::Sleep([int]$Test.DisconnectAfterMs)
    }

    return [PSCustomObject]@{
        Name               = $Test.Name
        Category           = $Test.Category
        Command            = $Test.Command
        Pass               = $pass
        Attempt            = $attempt
        Recovered          = $recovered
        TimeoutMs          = $Test.TimeoutMs
        FirstByteTimeoutMs = $firstByteTimeoutMs
        IdleTimeoutMs      = $idleTimeoutMs
        DisconnectAfterMs  = $Test.DisconnectAfterMs
        Note               = $Test.Note
        Tail               = $tail
    }
}

$paths = Get-ScriptPaths
$logDir = Join-Path $env:TEMP 'rl_sport_ai_fw_fast_validation'
if (-not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}
$timestamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$logPath = Join-Path $logDir ("rl_sport_ai_fw_fast_validation_{0}.log" -f $timestamp)
$logBuffer = New-Object System.Text.StringBuilder

Write-Host "[FAST] RL_SPORT root : $($paths.RlSportRoot)"
Write-Host "[FAST] log output    : $logPath"

if (-not $SkipBuild) {
    Write-Host '[FAST] Step 1/3: build debug firmware'
    & cbuild $paths.Csolution --context-set --packs
    Assert-LastExitCode 'cbuild'
}

$cbuildRun = Get-CbuildRunPath -CbuildIdxPath $paths.CbuildIdx -VsCodeRoot $paths.VsCodeRoot
Write-Host "[FAST] cbuild-run    : $cbuildRun"

if (-not $SkipLoad) {
    Write-Host '[FAST] Step 2/3: load firmware to target'
    & pyocd load --probe 'cmsisdap:' --cbuild-run $cbuildRun
    Assert-LastExitCode 'pyocd load'
    [System.Threading.Thread]::Sleep(500)
}

if (-not $PortName) {
    $PortName = Wait-NuLinkSerialPort -TimeoutMs 3000 -PollMs 100
}

Write-Host "[FAST] Step 3/3: run accelerated UART validation on $PortName @ $BaudRate"

$tests = @(
    @{ Name = 'INFO'; Command = 'AT+TEST=INFO'; Expect = '\+TEST:INFO,PASS'; TimeoutMs = 700; FirstByteTimeoutMs = 220; IdleTimeoutMs = 120; DisconnectAfterMs = 0; Category = 'auto'; Note = 'firmware identity / build stamp' },
    @{ Name = 'ADC'; Command = 'AT+TEST=ADC'; Expect = '\+TEST:ADC,PASS'; TimeoutMs = 700; FirstByteTimeoutMs = 220; IdleTimeoutMs = 120; DisconnectAfterMs = 0; Category = 'auto'; Note = 'battery ADC range check' },
    @{ Name = 'I2C_SCAN'; Command = 'AT+TEST=I2C,SCAN'; Expect = '\+TEST:I2C,PASS'; TimeoutMs = 900; FirstByteTimeoutMs = 250; IdleTimeoutMs = 140; DisconnectAfterMs = 0; Category = 'auto'; Note = 'sensor alive via I2C path' },
    @{ Name = 'GSENSOR_CAL'; Command = 'AT+TEST=GSENSOR,CAL'; Expect = '\+TEST:GSENSOR,PASS,CAL'; TimeoutMs = 2400; FirstByteTimeoutMs = 1700; IdleTimeoutMs = 180; DisconnectAfterMs = 0; Category = 'auto'; Note = '64-sample gravity calibration' },
    @{ Name = 'GSENSOR'; Command = 'AT+TEST=GSENSOR'; Expect = '\+TEST:GSENSOR,PASS,'; TimeoutMs = 1100; FirstByteTimeoutMs = 350; IdleTimeoutMs = 180; DisconnectAfterMs = 0; Category = 'auto'; Note = 'post-calibration motion baseline' },
    @{ Name = 'HALL_SNAPSHOT'; Command = 'AT+TEST=HALL'; Expect = '\+TEST:HALL,PASS,PB7='; TimeoutMs = 700; FirstByteTimeoutMs = 220; IdleTimeoutMs = 120; DisconnectAfterMs = 0; Category = 'auto'; Note = 'logical hall state snapshot (no magnet required)' },
    @{ Name = 'PWR_VBUS'; Command = 'AT+TEST=PWR,VBUS'; Expect = '\+TEST:PWR,PASS,VBUS='; TimeoutMs = 700; FirstByteTimeoutMs = 220; IdleTimeoutMs = 120; DisconnectAfterMs = 0; Category = 'auto'; Note = 'USB VBUS sense path' },
    @{ Name = 'PWR_LOCK'; Command = 'AT+TEST=PWR,LOCK'; Expect = '\+TEST:PWR,PASS,PA11='; TimeoutMs = 700; FirstByteTimeoutMs = 220; IdleTimeoutMs = 120; DisconnectAfterMs = 0; Category = 'auto'; Note = 'power-lock GPIO state' },
    @{ Name = 'LED_LOGIC'; Command = 'AT+TEST=LED,BLINK'; Expect = '\+TEST:LED,PASS,BLINK=3'; TimeoutMs = 1100; FirstByteTimeoutMs = 250; IdleTimeoutMs = 180; DisconnectAfterMs = 120; Category = 'logic-surrogate'; Note = 'firmware LED control path only; visual confirmation separated' },
    @{ Name = 'BUZZER_LOGIC'; Command = 'AT+TEST=BUZZER'; Expect = '\+TEST:BUZZER,PASS'; TimeoutMs = 900; FirstByteTimeoutMs = 250; IdleTimeoutMs = 180; DisconnectAfterMs = 120; Category = 'logic-surrogate'; Note = 'firmware buzzer path only; audible confirmation separated' },
    @{ Name = 'BLE_NAME'; Command = 'AT+TEST=BLE,NAME'; Expect = '\+TEST:BLE,PASS,NAME='; TimeoutMs = 3800; FirstByteTimeoutMs = 3000; IdleTimeoutMs = 250; DisconnectAfterMs = 150; Category = 'auto'; Note = 'BLE command mode + advertised name query' },
    @{ Name = 'BLE_MAC'; Command = 'AT+TEST=BLE,MAC'; Expect = '\+TEST:BLE,(PASS,MAC=|INFO,MAC=NA)'; TimeoutMs = 3200; FirstByteTimeoutMs = 2400; IdleTimeoutMs = 250; DisconnectAfterMs = 150; Category = 'auto'; Note = 'BLE MAC query; INFO,MAC=NA is accepted because some modules do not return a parseable MAC in CMD mode' }
)

if (-not $SkipLongTests) {
    $tests += @{ Name = 'USB_LOGIC'; Command = 'AT+TEST=USB'; Expect = '\+TEST:USB,PASS,DUR=5000'; TimeoutMs = 6200; FirstByteTimeoutMs = 5600; IdleTimeoutMs = 300; DisconnectAfterMs = 200; Category = 'logic-surrogate'; Note = 'runs HID motion generator; host enumeration is checked manually' }
}

if ($IncludeAllSmoke) {
    $tests += @{ Name = 'ALL_SMOKE'; Command = 'AT+TEST=ALL'; Expect = '\+TEST:ALL,DONE,'; TimeoutMs = 10000; FirstByteTimeoutMs = 350; IdleTimeoutMs = 2800; DisconnectAfterMs = 200; Category = 'auto'; Note = 'integrated board-level smoke aggregator' }
}

$serial = $null
$results = New-Object System.Collections.Generic.List[object]
$suiteStartTick = [Environment]::TickCount64
$suiteTimedOut = $false
try {
    foreach ($test in $tests) {
        if (([Environment]::TickCount64 - $suiteStartTick) -ge $SuiteTimeoutMs) {
            $suiteTimedOut = $true
            Write-Host "`n[FAST] Suite timeout reached (${SuiteTimeoutMs}ms); stopping remaining tests."
            break
        }

        $result = Invoke-FastTest -Test $test -PortRef ([ref]$serial) -PortName $PortName -PortNameRef ([ref]$PortName) -BaudRate $BaudRate -LogBuffer $logBuffer -DefaultFirstByteTimeoutMs $DefaultFirstByteTimeoutMs -DefaultIdleTimeoutMs $DefaultIdleTimeoutMs -CbuildRun $cbuildRun
        $results.Add($result) | Out-Null
    }

    Open-PortIfNeeded -PortRef ([ref]$serial) -PortName $PortName -BaudRate $BaudRate
    Reset-PortBuffers -Port $serial
    Write-Host "`n[FAST TX] AT+TEST=EXIT"
    $serial.Write("AT+TEST=EXIT`r`n")
    [void](Read-UntilMatch -Port $serial -TimeoutMs 700 -ExpectedPattern '\+TEST:EXIT,PASS' -LogBuffer $logBuffer -FirstByteTimeoutMs 250 -IdleTimeoutMs 120)
}
finally {
    Close-Port -PortRef ([ref]$serial)
    [System.IO.File]::WriteAllText($logPath, $logBuffer.ToString(), [System.Text.Encoding]::UTF8)
}

$autoPass = @($results | Where-Object { $_.Category -eq 'auto' -and $_.Pass }).Count
$autoFail = @($results | Where-Object { $_.Category -eq 'auto' -and -not $_.Pass }).Count
$logicPass = @($results | Where-Object { $_.Category -eq 'logic-surrogate' -and $_.Pass }).Count
$logicFail = @($results | Where-Object { $_.Category -eq 'logic-surrogate' -and -not $_.Pass }).Count

Write-Host "`n===== RL_SPORT FAST VALIDATION SUMMARY ====="
$results | Format-Table -AutoSize Name, Category, Pass, Attempt, Recovered, TimeoutMs, FirstByteTimeoutMs, IdleTimeoutMs, DisconnectAfterMs
Write-Host "AUTO_PASS=$autoPass"
Write-Host "AUTO_FAIL=$autoFail"
Write-Host "LOGIC_SURROGATE_PASS=$logicPass"
Write-Host "LOGIC_SURROGATE_FAIL=$logicFail"
Write-Host "LOG_PATH=$logPath"
Write-Host "SUITE_TIMEOUT_REACHED=$suiteTimedOut"

Write-Host "`n===== MANUAL / PHYSICAL SMOKE CHECKS (SEPARATED) ====="
Write-Host '1. LED visual: send AT+TEST=LED,BLINK and visually confirm PB3 blink x3.'
Write-Host '2. Buzzer audible: send AT+TEST=BUZZER and confirm tone is heard.'
Write-Host '3. USB host-side: send AT+TEST=USB and confirm host enumerates HID mouse + cursor moves.'
Write-Host '4. Key press: send AT+TEST=KEY,3000 then physically press PB15 within 3s.'
Write-Host '5. Hall edge: send AT+TEST=HALL,WAIT,3000 then move magnet across PB7 sensor.'

if ((-not $suiteTimedOut) -and (($autoFail + $logicFail) -eq 0)) {
    Write-Host 'FAST_VALIDATION_STATUS=PASS'
    exit 0
}

Write-Host 'FAST_VALIDATION_STATUS=FAIL'
exit 1
