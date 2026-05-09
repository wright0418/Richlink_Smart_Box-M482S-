param(
    [string]$Com = "COM3",
    [int]$Baud = 115200,
    [string]$LogFile = "",
    [switch]$ShowOnlyMole
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($LogFile)) {
    $ts = Get-Date -Format "yyyyMMdd_HHmmss"
    $LogFile = Join-Path $PSScriptRoot "mole_trace_$ts.log"
}

$sp = New-Object System.IO.Ports.SerialPort $Com, $Baud, ([System.IO.Ports.Parity]::None), 8, ([System.IO.Ports.StopBits]::One)
$sp.NewLine = "`n"
$sp.ReadTimeout = 250
$sp.DtrEnable = $true
$sp.RtsEnable = $true

Write-Host "[TRACE] Open $Com @$Baud"
Write-Host "[TRACE] Logging to: $LogFile"
Write-Host "[TRACE] Press Ctrl+C to stop"

$sp.Open()

try {
    while ($true) {
        try {
            $line = $sp.ReadLine()
        } catch [System.TimeoutException] {
            continue
        }

        if ($null -eq $line) { continue }
        $line = $line.Trim("`r", "`n")
        if ($line.Length -eq 0) { continue }

        $isMole = $line -match "\[MOLE_TEST\]"
        if ($ShowOnlyMole -and -not $isMole) { continue }

        $prefix = ""
        if ($line -match "\[MOLE_TEST\] RAW") { $prefix = "[RAW]" }
        elseif ($line -match "\[MOLE_TEST\] RX LED") { $prefix = "[RX_LED]" }
        elseif ($line -match "\[MOLE_TEST\] RX BRIGHTNESS_CMD") { $prefix = "[RX_BRI]" }
        elseif ($line -match "\[MOLE_TEST\] RX HIT_CONFIG") { $prefix = "[RX_CFG]" }
        elseif ($line -match "\[MOLE_TEST\] TX HIT=0x01") { $prefix = "[TX_HIT]" }

        $out = "[{0}] {1} {2}" -f (Get-Date -Format "HH:mm:ss.fff"), $prefix, $line
        Write-Host $out
        Add-Content -Path $LogFile -Value $out
    }
}
finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
    Write-Host "[TRACE] Closed"
}
