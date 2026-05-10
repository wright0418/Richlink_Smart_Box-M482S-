$ErrorActionPreference = 'Stop'

$compiler = Get-Command gcc -ErrorAction SilentlyContinue
$mode = "host"
if (-not $compiler) {
    $compiler = Get-Command clang -ErrorAction SilentlyContinue
}
if (-not $compiler) {
    $compiler = Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue
    if ($compiler) {
        $mode = "cross-compile-only"
    }
}
if (-not $compiler) {
    $compiler = Get-Command armclang -ErrorAction SilentlyContinue
    if ($compiler) {
        $mode = "cross-compile-only"
    }
}

if (-not $compiler) {
    Write-Error "No suitable compiler found. Install gcc/clang (host) or arm-none-eabi-gcc/armclang (compile check)."
}

$testTargets = @(
    @{
        Name    = "test_squat_detect"
        Sources = @(
            (Join-Path $PSScriptRoot "test_squat_detect.c"),
            (Join-Path $PSScriptRoot "..\app\algorithms\squat_detect.c")
        )
    },
    @{
        Name    = "test_game_algorithms"
        Sources = @(
            (Join-Path $PSScriptRoot "test_game_algorithms.c")
        )
    },
    @{
        Name    = "test_ble_parser"
        Sources = @(
            (Join-Path $PSScriptRoot "test_ble_parser.c"),
            (Join-Path $PSScriptRoot "..\protocol\ble_parser.c")
        )
    },
    @{
        Name    = "test_mole_packet"
        Sources = @(
            (Join-Path $PSScriptRoot "test_mole_packet.c"),
            (Join-Path $PSScriptRoot "..\protocol\mole_packet.c")
        )
    }
)

function Assert-LastExitCode {
    param(
        [string]$StepName
    )

    if ($LASTEXITCODE -ne 0) {
        throw "$StepName failed with exit code $LASTEXITCODE"
    }
}

foreach ($target in $testTargets) {
    $out = Join-Path $PSScriptRoot ("{0}.exe" -f $target.Name)

    if ($compiler.Name -eq 'gcc') {
        & gcc -std=c11 -Wall -Wextra -pedantic @($target.Sources) -o $out -lm
        Assert-LastExitCode "Build $($target.Name)"
    }
    elseif ($compiler.Name -eq 'clang') {
        & clang -std=c11 -Wall -Wextra -pedantic @($target.Sources) -o $out -lm
        Assert-LastExitCode "Build $($target.Name)"
    }
    elseif ($compiler.Name -eq 'arm-none-eabi-gcc') {
        foreach ($src in $target.Sources) {
            $objName = "{0}.{1}.o" -f $target.Name, [System.IO.Path]::GetFileNameWithoutExtension($src)
            & arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c11 -Wall -Wextra -pedantic -c $src -o (Join-Path $PSScriptRoot $objName)
            Assert-LastExitCode "Compile-only $($target.Name): $src"
        }
    }
    else {
        foreach ($src in $target.Sources) {
            $objName = "{0}.{1}.o" -f $target.Name, [System.IO.Path]::GetFileNameWithoutExtension($src)
            & armclang --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -std=c11 -Wall -Wextra -c $src -o (Join-Path $PSScriptRoot $objName)
            Assert-LastExitCode "Compile-only $($target.Name): $src"
        }
    }

    if ($mode -eq "host") {
        & $out
        Assert-LastExitCode "Run $($target.Name)"
    }
}

if ($mode -eq "host") {
    Write-Host "All host test targets passed."
}
else {
    Write-Host "Cross compiler detected ($($compiler.Name)). Compile-only check passed for $($testTargets.Count) test target(s)."
    Write-Host "Install host gcc/clang to execute tests on this machine."
}
