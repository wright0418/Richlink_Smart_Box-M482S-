# RL_SPORT 演算法單元測試

此目錄提供可在主機端執行的「純演算法」測試，覆蓋：

- movement 判定 (`GameAlgo_IsMovement`)
- Hall edge -> jump 計數 (`GameAlgo_CalcJumpsFromEdges`)

## 檔案

- `test_game_algorithms.c`：測試主程式
- `run_tests.ps1`：Windows PowerShell 測試執行腳本

## 需求

- `gcc` 或 `clang` 其中之一在 PATH 中

## 執行方式

```powershell
cd SampleCode/RL_SPORT/tests
./run_tests.ps1
```

## 設計說明

這些測試僅驗證演算法，不依賴 MCU 暫存器或中斷；目標是讓日常維護快速回歸，並在調整閾值/計數邏輯時降低風險。
