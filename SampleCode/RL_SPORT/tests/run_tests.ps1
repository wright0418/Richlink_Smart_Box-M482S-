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

$src = Join-Path $PSScriptRoot "test_game_algorithms.c"
$out = Join-Path $PSScriptRoot "test_game_algorithms.exe"

if ($compiler.Name -eq 'gcc') {
    & gcc -std=c11 -Wall -Wextra -pedantic $src -o $out -lm
}
elseif ($compiler.Name -eq 'clang') {
    & clang -std=c11 -Wall -Wextra -pedantic $src -o $out -lm
}
elseif ($compiler.Name -eq 'arm-none-eabi-gcc') {
    & arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -std=c11 -Wall -Wextra -pedantic -c $src -o (Join-Path $PSScriptRoot "test_game_algorithms.o")
}
else {
    & armclang --target=arm-arm-none-eabi -mcpu=cortex-m4 -mthumb -std=c11 -Wall -Wextra -c $src -o (Join-Path $PSScriptRoot "test_game_algorithms.o")
}

if ($mode -eq "host") {
    & $out
}
else {
    Write-Host "Cross compiler detected ($($compiler.Name)). Compile-only check passed."
    Write-Host "Install host gcc/clang to execute tests on this machine."
}
