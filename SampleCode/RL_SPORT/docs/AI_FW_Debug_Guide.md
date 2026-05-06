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

如需我把這份指南轉成 repository 的 `README.md`（或整合進 CI 文件），或改寫成 Python 版本的自動化腳本，我可以接著幫你完成。