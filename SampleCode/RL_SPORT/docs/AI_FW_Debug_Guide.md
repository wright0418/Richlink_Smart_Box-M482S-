# AI MCU FW Debug 自動化測試指南 (RL_SPORT)

## 概要
簡要說明：建立一組可重複的「build → load → auto-detect NuLink COM → open UART 115200 → send AT command → verify response」的自動化閉環，供 Copilot / 自動化腳本在開發或 CI（有硬體 runner）時參考。

本檔案描述目前在專案中已實作的方式（包含需要的檔案、執行範例、成功判定、常見問題與後續建議），方便未來讓 Copilot 依此產生自動化測試或延伸腳本。

## 前置條件
- 開發機：Windows（已驗證 PowerShell 5.1/PowerShell Core 可執行）
- 已安裝並可執行：`cbuild`、`pyocd`（CMSIS-DAP）、AC6 toolchain（或 repository 指定的 toolchain）
- NuLink2（或相容 CMSIS-DAP）已連接目標板並裝好驅動，能提供 Virtual COM Port
- 有權限開啟 COM port
- 專案路徑：`SampleCode/RL_SPORT`

## 主要變更 / 相關檔案
- 已修改：`SampleCode/RL_SPORT/project_config.h`
  - `#define DEBUG` 已設為 `1`（開啟 `DBG_PRINT()` 到 UART0）
- 新增：`SampleCode/RL_SPORT/tools/ai_fw_debug_loop.ps1`
  - PowerShell 腳本：自動 build / load / detect COM / open UART / 送 AT 指令並判定回應
- 日誌輸出位置（執行時）：`%TEMP%\rl_sport_ai_fw_debug\rl_sport_ai_fw_debug_<TIMESTAMP>.log`

## 腳本行為概述
- build：執行 `cbuild <csolution> --context-set --packs`（預設 debug context `RL_SPORT.debug+ARMCLANG`）
- load：使用 `pyocd load --probe 'cmsisdap:' --cbuild-run <cbuild-run.yml>` 將 firmware flash 到板上
- COM 偵測：利用 `Get-CimInstance Win32_SerialPort` 自動選取名稱/說明含 `NuLink` 或 `Virtual COM Port` 的裝置
- UART 操作：開啟 serial 並以 115200、8N1、無 handshake、`\r\n` newline 傳送/接收
- 成功判定：當收到包含 `+TEST:` 的串流（或 `OK`）即視為成功，腳本會儲存 log 並 exit 0

## 腳本參數（重點）
- `-PortName`：指定 COM port（例如 `COM13`），若不給會自動偵測
- `-BaudRate`：預設 `115200`
- `-SkipBuild`：跳過 build 階段（適合只測試串流）
- `-SkipLoad`：跳過 load 階段（適合已手動燒錄或使用模擬環境）
- `-StartupCaptureMs`：上電後先擷取的啟動 log 時間（ms），腳本預設跳過等待並立即送指令以避免板子自動省電
- `-CommandCaptureMs`：每次送指令後等待回應的最多時間（ms），預設約 1500ms
- `-Commands`：字串陣列，預設 `@('AT+TEST=INFO')`

## 使用範例
在 PowerShell 中執行（從 `SampleCode/RL_SPORT` 根目錄）：

```powershell
# 預設行為：build -> load -> auto detect COM -> send AT+TEST=INFO
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ai_fw_debug_loop.ps1

# 指定 COM 並發送多個命令（跳過 build/load）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ai_fw_debug_loop.ps1 -PortName COM13 -SkipBuild -SkipLoad -Commands 'AT+TEST=INFO','AT+TEST=ADC'
```

### 期望輸出（成功）
腳本在完成時會印出：

```
[AI-FW] Closed-loop UART debug succeeded. Log saved to: C:\Users\<you>\AppData\Local\Temp\rl_sport_ai_fw_debug\rl_sport_ai_fw_debug_<TIMESTAMP>.log
```

在 log 或 console 中會看到：
- `[UART TX] AT+TEST=INFO`
- `[UART RX] +TEST:INFO,PASS,FW=1.4.0,BRD=RL_SPORT_V3,BUILD=...`
- `OK`

如果沒有出現 `+TEST:` 或 `OK`，腳本會回報錯誤並把完整 log 存檔以供追查。

## 常見問題與排查建議
- 腳本找不到 NuLink COM
  - 確認 NuLink2 已接上且裝置管理員可見 Virtual COM Port
  - 手動在 PowerShell 用 `Get-CimInstance Win32_SerialPort` 檢查
- pyOCD load 失敗
  - 嘗試單獨執行 `pyocd load --probe 'cmsisdap:' --cbuild-run <file>` 檢查錯誤訊息
  - 確認 pyOCD 對應的 probe support/NuLink 版本
- 板子很快進入 power-off（觀察到大量 `[BATT] vdda=3.05V LOW`）
  - 臨時解法：在腳本上發送指令時不要等待太久，或使用 `-SkipLoad` 與已通電的板子
  - 穩健作法：新增 debug profile（在 firmware 中）讓 debug 模式下延長或停用 idle->power-off 流程
  - 若可行，給板子較高的供電或充飽電以避免早期關機

## 對 Copilot 的建議（如何自動撰寫測試）
以下條目可作為 Copilot 在生成測試時的參考：

- 單元測試（Firmware）
  - 測試 `AT+TEST` 指令解析與回應格式化的 C 函式，透過 repository 既有的 unit test framework（若有）建立測資，如：

```c
// 測試範例（concept）
void test_AT_Info_format(void) {
  char buf[128];
  AT_Info(buf, sizeof(buf));
  assert(strstr(buf, "+TEST:INFO") != NULL);
}
```

- 模擬整合測試（Host-side）
  - 寫一個小型的 serial emulator（Python）來模擬 MCU 回應，讓 `ai_fw_debug_loop.ps1` 在沒有真實硬體時也能驗證腳本邏輯
  - Python emulator 範例思路：打開一個 COM pair（或使用虛擬 serial 軟體），等待指令，收到 `AT+TEST=INFO` 回 `+TEST:INFO,...` 然後關閉

- 硬體整合測試（HIL）
  - 在有硬體 runner（實驗室或 CI runner）的情況下，將腳本納入 pipeline；若 pipeline 支援，使用 `-SkipBuild`/`-SkipLoad` 跳過耗時步驟或重用已編譯 artifact

- 成功判定邏輯
  - 以出現 `+TEST:` token 為主判定條件（比完整等待 `OK` 更穩健，因為目標可能在回應後很快 power-off）
  - 若需更嚴格，可指定 `+TEST:INFO` 與緊接的 `OK`

## 建議的後續改進（短中長期）
- 在 firmware 加入 `DEBUG_PROFILE` compile-time 選項：
  - 在該模式下延長 idle timeout或停用自動 power-off，並輸出更多 debug log
- 將 `ai_fw_debug_loop.ps1` 改寫成 cross-platform（Python）版本，方便在 Linux/macOS runner 運行
- 新增一個 `tools/mock_serial` 來做 Host-side emulator，供 Copilot 生成的整合測試直接呼叫
- 將此腳本納入 repo 的 `Makefile` / `ci` 目標，並建立 `ci/hardware/README.md` 說明如何在有硬體 runner 的 CI 中執行

## 日誌路徑
執行時會把 log 存於（範例）：

```
C:\Users\Wright\AppData\Local\Temp\rl_sport_ai_fw_debug\rl_sport_ai_fw_debug_20260507_061123.log
```

## 範例 commit message（建議）
```
tools: add ai_fw_debug_loop.ps1 and enable DEBUG in project_config.h (add debug UART closed-loop automation)
```

---

## 本次自動化測試的修正與注意事項（2026-05-07）

以下為本次執行 `ai_fw_fast_validation.ps1` / `ai_fw_debug_loop.ps1` 所發現並已修正或加強的要點，請日後依此操作或讓 Copilot 依此產生測試腳本：

- 主要修正摘要：
  - 在 `tools/ai_fw_fast_validation.ps1` 與 `tools/ai_fw_debug_loop.ps1` 中加入：
    - COM open 重試與 `Open-PortIfNeeded`（MaxOpenAttempts=3, OpenRetryDelayMs=150）
    - `Read-UntilMatch`：採用 FirstByteTimeout / IdleTimeout，且偵測 `+TEST:...,FAIL` 或 `ERROR` 以提早判定失敗
    - 自動復原 `Reset-TargetByLoad`（在讀取例外或無回應時以 `pyocd load` 重灌並等待 Port 重現）
    - `Wait-NuLinkSerialPort`：在重灌後等待 Virtual COM 重現（預設 3000 ms）以避免立刻打開造成 I/O aborted

- Timeout 與重試行為（參數來源於腳本）：
  - 全套 suite 預設：`-SuiteTimeoutMs 25000`（可用參數覆蓋）
  - 全域預設：`DefaultFirstByteTimeoutMs = 250 ms`，`DefaultIdleTimeoutMs = 180 ms`
  - 個別測試可 override（在 `$tests` 內定義）。例：
    - `BLE_NAME` 首字節等待 3000 ms
    - `BLE_MAC` 首字節等待 2400 ms
    - `USB_LOGIC` 首字節等待 5600 ms（長測試，預設可由 `-SkipLongTests` 跳過）
    - `ALL_SMOKE` IdleTimeout 可達 2800 ms（用於整合 smoke）

- Recovery 行為說明：
  - 當 `Read-UntilMatch` 發現：
    - 讀取例外 (ReadExceptionMessage)、或
    - 首字節逾時 (FirstByteTimedOut)、或
    - 空回應 (no UART response)
    時，將觸發一次 `Reset-TargetByLoad`（預設 `MaxRecoveryAttempts = 1`）；此機制會：關閉 serial、執行 `pyocd load`、等待 `SettleMs`（預設 300 ms）、再等候 Virtual COM 重現（3s）。
  - 若不穩定可考慮調整 `SettleMs` 至 500–1000 ms，或把 `MaxRecoveryAttempts` 調高（但會拉長總測試時間）。

- BLE_MAC 行為觀察與建議：
  - 觀察：單獨執行 `AT+TEST=BLE,MAC` 時偶有 `NO_RESPONSE`；但以 `AT+TEST=ALL` 執行時（流程內可能含等待/重試），整體 `ALL` 有時會回報 PASS。
  - 建議：
    - 在 CI/快速驗證中，把 `BLE_MAC` 視為非阻斷項目（允許暫時的 NO_RESPONSE），以 `AT+TEST=ALL` 作為 smoke 判定；
    - 若需嚴格檢查，將該測試的 `FirstByteTimeoutMs` 拉高（例如 3000 ms 以上），或在 FW 端強化在 command-mode 下立即回應 MAC 查詢。

- 常見故障排查快速指南：
  - 未偵測到 NuLink COM：手動指定 `-PortName COMx`，或在 Device Manager 確認驅動與線材。
  - pyOCD load 失敗：單獨執行 `pyocd load --probe 'cmsisdap:' --cbuild-run <file>` 檢查錯誤。
  - ReadExisting 時 I/O aborted：把 `Reset-TargetByLoad` 的 `SettleMs` 提高到 500–1000 ms，或拉長 `Wait-NuLinkSerialPort` timeout。
  - 首字節 (FirstByte) 經常逾時：提高 `-DefaultFirstByteTimeoutMs` 或針對該 test 在 `$tests` 調整 `FirstByteTimeoutMs`。

- 日誌與路徑：
  - `ai_fw_fast_validation.ps1` 會將完整 log 存於：

```
%TEMP%\rl_sport_ai_fw_fast_validation\rl_sport_ai_fw_fast_validation_<timestamp>.log
```

- 常用啟動範例：
  - 跳過 build/load（已手動燒錄或模擬）並略過長測試：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ai_fw_fast_validation.ps1 -SkipBuild -SkipLoad -PortName COM13 -SkipLongTests
```

  - 完整執行（含重灌與長測試）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\ai_fw_fast_validation.ps1
```

- 建議的 Commit message（中文範例）：

```
docs: update AI_FW_Debug_Guide — add fixes, timeout and recovery notes for fast validation
```

如需我把這些變更同步 commit（或把整份指南轉成 repo README / CI 文件），或是把 PowerShell 腳本改寫成 cross-platform 的 Python 版本，我可以接著幫你完成。

如需我把這份指南轉成 repository 的 `README.md`（或整合進 CI 文件），或改寫成 Python 版本的自動化腳本，我可以接著幫你完成。