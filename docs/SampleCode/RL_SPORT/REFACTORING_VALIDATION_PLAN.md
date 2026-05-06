# RL_SPORT 系統整理與逐步驗證計畫

> 目標：在「功能維持正常」的前提下，讓 `SampleCode/RL_SPORT` 更有條理，後續新增功能、定位問題與回歸驗證都能更快、更安全。

## 目前基準狀態

本文件建立時的零改動驗證結果：

- Git 工作樹：原始檢查時無未提交變更。
- VS Code Problems：`SampleCode/RL_SPORT` 無錯誤回報。
- CMSIS/AC6 韌體建置：成功。
  - Solution：`SampleCode/RL_SPORT/VSCode/RL_SPORT.csolution.yml`
  - Compiler：AC6 V6.24.0
  - 結果：`1 succeeded, 0 failed`，且 Ninja 回報 no work to do。
- Host-side 演算法測試：目前此機器偵測到 `arm-none-eabi-gcc.exe`，完成 cross compile-only check。
  - 腳本：`SampleCode/RL_SPORT/tests/run_tests.ps1`
  - 備註：若要真的執行 host binary，需額外安裝 host `gcc` 或 `clang`。

## 已完成進度（截至 2026-05-07）

- **Phase 1 已完成**
  - `SampleCode/RL_SPORT/README.md` 已補上 build/test 快速入口、UART0 `AT+TEST=` 與 BLE `AT+TEST,` 測試通道說明。
  - `docs/SampleCode/RL_SPORT/RL_SPORT_FW_規格.md` 已同步目前實作：`Adc_Init()`、`ADC_VDDA_LOW_V`、charge mode 行為、BLE rename flow 與測試入口。
- **Phase 2 已完成**
  - `ble.c` 中重複的 `BLE_CMD_CCMD` 清理與 REPL 前置處理已收斂成單一 helper。
  - `ble.c` 原先直接寫入 `g_sys.*` 的路徑已改為 `Sys_*()` API。
  - MAC / device name 清空已改為語意化的 `Sys_ClearMacAddr()` / `Sys_ClearDeviceName()`。
- **Phase 3 核心項目已完成**
  - 純文字解析已拆到 `protocol/ble_parser.c/h`。
  - 新增 `tests/test_ble_parser.c`。
  - `tests/run_tests.ps1` 已擴充為 2 個 target（演算法 + BLE parser）。
  - `SampleCode/RL_SPORT/VSCode/RL_SPORT.cproject.yml` 已納入 `protocol/ble_parser.c`。
  - Phase 5 第一批已完成：`ble_parser.*` 已搬入 `protocol/`，對應 include 與 host test/build 路徑已同步更新。
- **Phase 4 第一波已完成**
  - `system_status.c/h` 已新增 `Sys_CopyMacAddr()`、`Sys_CopyDeviceName()`。
  - `ble.c` rename flow 與 `test_mode.c` 已改為讀取字串快照，不再直接持有內部 buffer pointer。
  - 舊的字串 pointer getter 已移除，改以 copy-out / clear API 為主。
  - `jump_times` 的寫入已收斂到 `system_status.c`（`Sys_SetJumpTimes()` / `Sys_AddJumpTimes()`）。
  - 剩餘的 state accessor 已從 `system_status.h` inline 收斂到 `system_status.c`，`g_sys` 不再由 header 外露。
  - 未使用的 `keyA_state` / `left_time_ms` 欄位已移除，相關註解同步修正。
- **截至目前的驗證結果**
  - VS Code Problems：相關檔案持續無錯誤。
  - `tests/run_tests.ps1`：compile-only check 持續通過 2 個 target。
  - CMSIS/AC6：多次重建成功。
  - 本輪 `system_status` accessor 收斂後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `protocol/ble_parser.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `drivers/timer.*`、`drivers/adc.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `drivers/i2c.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `board/power_mgmt.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `board/gpio.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `board/board_test_gpio.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `drivers/gsensor.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `drivers/led.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `drivers/buzzer.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `board/usb_hid/*` 搬移並修正遺留 `board_test_gpio.c` include 後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 本輪 `app/algorithms/hall_anticheat.*` 搬移後再次驗證：`tests/run_tests.ps1` compile-only 2 個 target 通過，`cbuild` 成功（Code=`33432`, RO-data=`2996`, RW-data=`104`, ZI-data=`7136`）。
  - 板上 smoke test / BLE 實機驗證：仍需在有硬體時依本文流程執行。

## 現有模組分層

建議維持並強化下列分層：

1. **Board / clock / pin 層**
   - `main.c`：`SYS_Init()`, `RL_InitSystemCore()`, 初始化順序 orchestration
  - `board/gpio.c/h`：MFP、GPIO、Power Lock、USB detect、wake pin helper
  - `board/power_mgmt.c/h`：SPD/DPD、charge mode、wake/release flow
  - `board/usb_hid/usb_hid_mouse.*`：USB FS HID test helper 與 descriptor（板級 USB clock/pin/PHY 設定）
2. **Driver / peripheral 層**
  - `drivers/timer.c/h`：1 ms tick、`delay_ms()`、timeout helper
  - `drivers/i2c.c/h`：I2C wrapper、retry、debug log gate
   - `drivers/gsensor.c/h`：MXC400 sensor init/read/power
  - `drivers/adc.c/h`：VDDA / low-battery
   - `drivers/led.c/h`, `drivers/buzzer.c/h`
3. **System state 層**
   - `system_status.c/h`：BLE/game/key/hall/idle/repl 狀態
4. **Protocol / test 層**
   - `ble.c/h`：UART1 BLE transport、AT response parser、rename FSM
   - `ble_at_repl.c/h`：BLE `AT+TEST,` REPL
   - `test_mode.c/h`：UART0 `AT+TEST=` board auto-test
    - `board/board_test_gpio.c/h`：板測流程
5. **Application / game 層**
   - `game_logic.c/h`：idle/movement 判定、BLE jump count 上報
    - `gsensor_jump_detect.c/h`, `app/algorithms/hall_anticheat.c/h`：可選演算法
   - `main.c`：主迴圈整合與狀態調度

## 已觀察到的整理重點

### 高價值、低風險

- `docs/SampleCode/RL_SPORT/RL_SPORT_FW_規格.md` 與 `SampleCode/RL_SPORT/README.md` 的 drift 已完成第一輪同步；後續重點改為每次小重構都一起維護文件，避免再次漂移。
- `SampleCode/RL_SPORT/legacy/pwm_timer.c/h` 已移動到 `SampleCode/RL_SPORT/legacy/` 並在 header 加註 DEPRECATED 註記；此模組為 placeholder/legacy，未被 `RL_SPORT.cproject.yml` 納入，也未被其他模組引用。

### 功能不變的小型程式整理候選

- `SampleCode/RL_SPORT/ble.c`
  - `CheckBleRecvMsg()` 內重複的 marker strip / REPL dispatch 已整理為單一 helper。
  - 直接操作 `g_sys.*` 的路徑已收斂到 `Sys_*()` API。
  - BLE parser 已拆成 `protocol/ble_parser.c/h`；若後續還要再切，下一個候選是把 rename FSM 再進一步純化或做更細的 host test。
- `SampleCode/RL_SPORT/system_status.c/h`
  - 已新增 `Sys_CopyMacAddr()` / `Sys_CopyDeviceName()` 與 `Sys_ClearMacAddr()` / `Sys_ClearDeviceName()`；舊的字串 pointer getter 已移除。
  - `jump_times` 與字串 buffer 的寫入已集中在 `system_status.c`；剩餘 state getter/setter 也已移到 `.c`，header 不再暴露 `g_sys`。

### 中長期結構風險

- 初始化順序高度重要：`SYS_Init()`、MFP、clock、`Timer_Init()`、UART、BLE、G-sensor 之間存在隱性依賴。重構時不要一次搬動初始化流程。
- 測試流程裡有多個 blocking wait / `delay_ms()`，這在 test mode 可接受，但應避免滲入正常遊戲主迴圈。
- `test_mode.c` 與 `ble_at_repl.c` 都承擔測試協定功能，建議文件明確區分：
  - UART0：產線/板測 `AT+TEST=`
  - BLE：遠端 REPL `AT+TEST,`

## 分階段整理路線

### Phase 0 — 鎖定基準（已完成初步）

目的：確認整理前功能與建置基準可重複。

驗證項目：

- CMSIS/AC6 build 成功。
- `tests/run_tests.ps1` 至少可 compile-only；若有 host gcc/clang，需執行並通過。
- VS Code Problems 無新增錯誤。
- 工作樹變更清楚可追蹤。

### Phase 1 — 文件與驗證手冊同步（已完成）

目的：不改功能，先讓團隊知道「怎麼 build、怎麼測、怎麼判斷 pass」。

建議變更：

- 更新 `SampleCode/RL_SPORT/README.md`：加入 build / host test / board test 快速入口。
- 修正 `docs/SampleCode/RL_SPORT/RL_SPORT_FW_規格.md` 與現況不一致的 API/常數/charge mode 描述。
- 補一張「UART0 `AT+TEST=` vs BLE `AT+TEST,`」對照表。
- 標註 `pwm_timer` 為 placeholder，檔案已移動到 `SampleCode/RL_SPORT/legacy/` 並標註為 deprecated；若確認不再使用，再另開 phase 移除。

已完成項目：

- `README.md` 已補上建置/測試入口與最小回歸流程。
- `RL_SPORT_FW_規格.md` 已與目前實作同步。
- UART0 與 BLE 測試通道對照已寫入文件。

驗證門檻：

- 韌體 build 必須仍成功。
- 不應有功能程式碼 diff。
- 文件中的檔名/API 名稱需與程式碼搜尋結果一致。

### Phase 2 — 低風險程式整理（已完成）

目的：只做「重複邏輯消除」與「accessor 一致化」，不調整狀態機行為。

建議變更：

- 在 `ble.c` 抽出 helper，例如：
  - strip `BLE_CMD_CCMD` marker
  - 先交給 `BleAtRepl_HandleMessage()`
  - 再進入 `BleParser_ParseCommand()`
- 將 `ble.c` 內直接 `g_sys.*` 寫入逐步替換為 `Sys_Set*()` API。
- 將 MAC/name 清空改為 `Sys_ClearMacAddr()` / `Sys_ClearDeviceName()`，避免外部直接碰 buffer。

已完成項目：

- `ble.c` 已抽出前置 normalize/repl helper。
- `ble.c` 內直接 `g_sys.*` 寫入已收斂至 accessor。
- reset/clear 路徑已換成語意化 clear API。

驗證門檻：

- CMSIS/AC6 build 成功。
- `tests/run_tests.ps1` compile-only 或 host run 通過。
- 板上 UART0 至少跑：
  - `AT+TEST=INFO`
  - `AT+TEST=ALL`
  - `AT+TEST=BLE,NAME`
  - `AT+TEST=BLE,MAC`
- BLE 端至少跑：
  - `AT+TEST,PING`
  - `AT+TEST,VERSION`
  - `AT+TEST,STATUS`
  - `AT+TEST,REPL_START`
  - `AT+TEST,REPL_STOP`

### Phase 3 — BLE parser / rename flow 可測試化（核心項目已完成）

目的：讓 BLE module 回覆格式變動時能用 host test 先抓問題。

建議變更：

- 將純字串解析邏輯從 `ble.c` 拆出，例如：
  - `protocol/ble_parser.c/h`
  - `BleParser_ParseCommand()`
  - `BleParser_ExtractMacSuffix4()`
  - `BleParser_ExtractRopeSuffix4()`
- 新增 host test：`tests/test_ble_parser.c`
- 保持 UART/硬體操作仍留在 `ble.c`。

已完成項目：

- `protocol/ble_parser.c/h` 已建立並納入 firmware build。
- `tests/test_ble_parser.c` 已建立。
- `tests/run_tests.ps1` 已可驗證 2 個 test target。
- rename FSM 仍保留在 `ble.c`，符合「parser 純化、硬體操作留在 transport 檔」的原始策略。

驗證門檻：

- 新增 parser host tests，涵蓋：connected/disconnected、CMD/DATA mode、`get cycle`、`set end`、MAC suffix、ROPE suffix、echo filtering。
- CMSIS/AC6 build 成功。
- 板上 BLE rename 仍符合 `ROPE_XXXX`。

### Phase 4 — System status API 收斂（第一波已完成）

目的：降低 `g_sys` 直接耦合，讓未來加功能或導入 RTOS/更複雜 ISR 資料流更安全。

建議變更：

- 先禁止新程式直接寫 `g_sys.*`，除 `system_status.c/h` 外都走 API。
- 新增 copy-out API，例如 `Sys_CopyMacAddr()`、`Sys_CopyDeviceName()`。
- 檢查 multi-byte 或 buffer 欄位是否需要 critical section。

已完成項目：

- `g_sys\.` 在 `system_status.*` 外的直接使用已清乾淨。
- `Sys_CopyMacAddr()` / `Sys_CopyDeviceName()` 與 clear API 已落地。
- `ble.c` rename flow、`test_mode.c` 已改用字串快照。
- 未用字串 pointer getter 已移除。
- `jump_times` 的 set/reset/increment 寫入已集中到 `system_status.c`。
- `system_status.h` 剩餘 inline accessor 已移到 `system_status.c`，`g_sys` 現在是 translation-unit private。
- 未使用的 `keyA_state` / `left_time_ms` 已從 `SystemStatus` 移除。

驗證門檻：

- 搜尋 `g_sys\.`：除 `system_status.*` 與必要 header inline 外，直接操作數量應下降。
- CMSIS/AC6 build 成功。
- UART0/BLE 測試同 Phase 2。

### Phase 5 — HAL boundary 與目錄整理

目的：將板級/硬體相依與應用邏輯更清楚分界，方便換板或新增功能。

目前進度：

- 第一批已完成：`ble_parser.*` 已搬移到 `protocol/`，`ble.c` / host test / `RL_SPORT.cproject.yml` / 文件皆已同步更新並完成 build/test 驗證。
- 第二批已完成：`timer.*`、`adc.*`、`i2c.*` 已搬移到 `drivers/`，include 與 `RL_SPORT.cproject.yml` 已同步，build/test 驗證通過。
- 第三批已完成：`power_mgmt.*`、`gpio.*`、`board_test_gpio.*` 已搬移到 `board/`，include 與 `RL_SPORT.cproject.yml` 已同步，build/test 驗證通過。
- 第四批已完成：`gsensor.*`、`led.*`、`buzzer.*` 已搬移到 `drivers/`，include 與 `RL_SPORT.cproject.yml` 已同步，build/test 驗證通過。
- 第五批已完成：`usb_hid_mouse.*`、`usb_hid_mouse_desc.c`、`usb_hid_mouse_internal.h` 已搬移到 `board/usb_hid/`，include 與 `RL_SPORT.cproject.yml` 已同步，build/test 驗證通過。
- 第六批已完成：`hall_anticheat.*` 已搬移到 `app/algorithms/`，include 與 `RL_SPORT.cproject.yml` 已同步，build/test 驗證通過。

建議方向：

- 不建議一次搬目錄；先以 header/API boundary 收斂。
- 若要搬，建議後續分成：
  - `app/`：`game_logic.*`, optional algorithms（例如 `app/algorithms/hall_anticheat.*`）
  - `drivers/`：`gsensor.*`, `i2c.*`, `adc.*`, `led.*`, `buzzer.*`, `timer.*`
  - `board/`：`gpio.*`, power/pin helpers, `usb_hid/` board test USB helper
  - `protocol/`：`ble.*`, `ble_at_repl.*`, parser
  - `tests/`：host tests
- 建議實際搬移順序（由低風險到高風險）：
  1. `protocol/`：先搬 `ble_parser.*`（純文字解析、已可 host test，已完成）。
  2. `drivers/`：再搬 `timer.*`、`adc.*`、`i2c.*` 這類相對獨立的周邊 wrapper（已完成）。
  3. `board/`：之後處理 `gpio.*`、`power_mgmt.*` 這類板級相依較高的模組（已完成）。
  4. 最後才碰 `main.c`、`ble.c`、`test_mode.c` 這些 orchestration / protocol glue 檔案。
- 每搬一小批就更新 `RL_SPORT.cproject.yml` 並立即 build。

驗證門檻：

- 每次搬檔後 CMSIS/AC6 build 成功。
- 產物大小/連結符號無非預期大幅變化。
- 板上 smoke test 通過。

## 建議每次變更後的驗證順序

1. 靜態檢查：VS Code Problems / include path 是否正常。
2. Host test：`SampleCode/RL_SPORT/tests/run_tests.ps1`。
3. CMSIS build：`SampleCode/RL_SPORT/VSCode/RL_SPORT.csolution.yml`。
4. Flash / board smoke：先 `AT+TEST=INFO`，再 `AT+TEST=ALL`。
5. BLE 協定：rename、connect/disconnect、`get cycle`、`set end`、BLE REPL。
6. 回歸確認：跳繩 HALL 計數、idle power-off、low battery LED、USB charge mode。

## 目前建議的下一步

目前建議先做 **Phase 5 小批次 boundary 收斂**，而不是直接搬大批目錄：

- `system_status` 的狀態封裝已完成第二波收斂；`protocol/ble_parser.*` 也已完成第一批搬移，下一步可評估 `drivers`（`timer.*`、`adc.*`、`i2c.*`）是否適合進行第二批搬移。
- `drivers` 第二批與 `board` 第三批（`power_mgmt.*`、`gpio.*`）已完成；建議下一步評估是否要將 `gsensor.*` 與 `board_test_gpio.*` 的邊界再收斂，或先停在目前穩定狀態。
- `pwm_timer` 已移動到 `legacy/` 並標註為 deprecated，建議暫時保留於 legacy 資料夾；若未來確定不用再刪除。
- 在有硬體時，依本文驗證順序補齊 UART0 / BLE / rename / power-mode 的 smoke test 紀錄。
- 等上述都穩定後，再進 Phase 5 做 HAL boundary / 目錄整理，風險會低很多。
