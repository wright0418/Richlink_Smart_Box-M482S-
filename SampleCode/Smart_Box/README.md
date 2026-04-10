# Nuvoton M480 智慧裝置範例與 SDK（Smart_Box 項目）

本段落先說明 Smart_Box 的軟體架構與主要路徑，再說明硬體架構（各 IO 與對外設備連接），最後指向 `main.c` 的使用方式與本文件目的。

簡短目標：讓開發者能快速定位程式碼模組、了解硬體接線與如何從 `main.c` 開始執行與除錯。

軟體架構（主要路徑與模組）

- `SampleCode/Smart_Box/`：範例主目錄
	- `main.c`：程式入口，負責系統初始化、模組整合與主迴圈（見 `SampleCode/Smart_Box/main.c`）
	- `ble_mesh_at.c/.h`：BLE Mesh AT 模組驅動與指令處理
	- `mesh_handler.c/.h`：Mesh 層的事件處理與狀態管理
	- `mesh_modbus_agent.c/.h`：透過 Mesh 傳送/接收 Modbus RTU 命令的代理
	- `modbus_sensor_manager.c/.h`、`modbus_rtu/`：Modbus RTU 感測器整合與範例
	- `digital_io.c/.h`：按鍵、DI/DO、PA.6 繼電器（或繼電器控制）邏輯與 debounce
	- `led_indicator.c/.h`：LED 指示燈控制（綁定、Provision、錯誤狀態顯示）
	- `DIGITAL_IO_GUIDE.md`、`AGNETS.md`：硬體對照與專案規範文件

以上模組大多以 callback 與 state-machine 方式互動，`main.c` 在初始化後會註冊 callback 並在主迴圈呼叫 update 函式。

硬體架構（主要 IO 與對外設備）

以下依 `SampleCode/Smart_Box/AGNETS.md` 與程式註解彙整（以 MCU 腳位為主）：

- MCU: Nuvoton M482 系列
- 時鐘：外接 HXT 12MHz 與 RTC 32.768kHz
- UART 與外設：
	- UART1 (PA.9 TX, PA.8 RX) — 115200 連接 BLE Mesh AT Module（AT 指令）
	- UART0 (PD.3 TX, PD.2 RX) — 9600 連接 RS485/Modbus RTU 裝置
	- I2C0 (PB.2 SCL, PB.3 SDA) — 連接 24LC64 EEPROM

- 重要 GPIO（依 AGNETS.md 與 code）：
	- PB.3 — BLUE LED
	- PB.2 — YELLOW LED
	- PB.1 — RED LED
	- PB.15 — 按鍵 A（KeyA，active low，長按觸發綁定/解除綁定行為）
	- PB.14 — RS485 DIR（方向控制，驅動 RS485 發送/接收）
	- PB.7 — Digital Input 1 (DI)
	- PA.6 — Relay control / DO（高電位為 ON，具自動關閉功能於 `digital_io` 模組）

注意事項：實際電路請以硬體設計檔與 `SampleCode/Smart_Box/DIGITAL_IO_GUIDE.md` 為準；不同板子或修訂可能導致 MFP 設定變更。

`main.c` 使用說明與本文件目的

- 位置：`SampleCode/Smart_Box/main.c`
- 主要功能：
	- 呼叫 `sys_init()` 設定系統時鐘、UART、多功能腳（MFP）等
	- 初始化 `digital_io_init()`、`led_indicator_init()`、`mesh_handler_init()`、`modbus_sensor_manager_init()` 與 `mesh_modbus_agent_init()` 等模組
	- 註冊 callback（例如按鍵長按觸發 BLE Mesh 的 unbind 命令、DI 變化回報到 Mesh）
	- 設定 SysTick 1ms，主迴圈負責呼叫 `digital_io_update()`、`led_indicator_update()`、`ble_mesh_at_update()` 與 agent/handler 的 poll

- 使用方法：
	1. 參考「快速上手」章節進行 build 與 flash。
	2. 透過序列埠（UART1）連接 BLE Mesh AT module，可用 AT 指令交互測試；若需要偵錯，啟用 GDB server 並從 VS Code 連線。
	3. 若要修改 IO 行為，編輯 `digital_io.c/.h`；新增或變更驅動時請更新 `SampleCode/Smart_Box/VSCode/*.csolution.yml` 中的 `groups`。

- 本文件目的：提供一份對 Smart_Box 專案的快速導覽，包含軟體模組定位、硬體腳位對應、以及如何從 `main.c` 追蹤程式執行流程與進行常見的建置/燒錄/偵錯工作。


## 目錄結構（摘要）

- Library/: CMSIS、Device headers、StdDriver 與其他共用程式庫。
- SampleCode/: 各式 Nuvoton 範例專案，包含 `Smart_Box/`、`FreeRTOS/`、`ISP/` 等。
- SampleCode/Smart_Box/: Smart Box 範例主程式、文件與實作檔案（`main.c`, `digital_io.c`, `mesh_handler.c` 等）。

完整範例與檔案請查看 `SampleCode/Smart_Box/` 目錄。

## 開發先決條件

- CMSIS-Toolbox（含 cbuild/csolution 支援）
- pyOCD（用於燒錄與 gdbserver）
- 支援的工具鏈：ARMCLANG (AC6) 與 GCC（範例 csolution 已同時支援）
- 建議在 Windows 上使用 VS Code 並安裝 CMSIS-Toolbox 擴充套件

## 建置與燒錄（常用步驟）

專案已提供 VS Code tasks（使用 pyOCD + cbuild 產生的 cbuild-run），常見流程如下：

1. 產生 cbuild 與目標設定（若尚未執行）

	在專案根目錄或 `SampleCode/Smart_Box/VSCode/` 中使用 cbuild，或透過 CMSIS-Toolbox 的 GUI 操作。

2. 燒錄 (Load)

	VS Code 內建的 task 範例名稱：`CMSIS Load`。
	若以命令列示範（PowerShell）：

	```powershell
	pyocd load --probe cmsisdap: --cbuild-run "SampleCode/Smart_Box/VSCode/out/Smart_Box+ARMCLANG.cbuild-run.yml"
	```

3. 啟動 GDB 伺服器並執行

	可使用 VS Code task：`CMSIS Run`，或直接：

	```powershell
	pyocd gdbserver --probe cmsisdap: --connect attach --persist --reset-run --cbuild-run "SampleCode/Smart_Box/VSCode/out/Smart_Box+ARMCLANG.cbuild-run.yml"
	```

4. 若要清除 (Erase)

	使用 task `CMSIS Erase` 或：

	```powershell
	pyocd erase --probe cmsisdap: --chip --cbuild-run "SampleCode/Smart_Box/VSCode/out/Smart_Box+ARMCLANG.cbuild-run.yml"
	```

註：上述路徑與 cbuild-run 檔名會依您在 VSCode 內選擇的 toolchain（ARMCLANG / GCC）與建置型態（debug/release）而不同，請以 `SampleCode/Smart_Box/VSCode/out/` 下的實際檔名為準。

## Smart_Box 範例重點

- 入口：`SampleCode/Smart_Box/main.c`（已有 `SYS_Init()` 與 UART、GPIO 初始化範例）
- 功能：BLE Mesh AT 代理、數位 I/O 管理、Modbus 文件、LED 指示與 UART AT 指令處理等（相關檔案位於 `SampleCode/Smart_Box/`）
- 硬體對應與 IO：請參考 `SampleCode/Smart_Box/AGNETS.md` 與專案內的 MFP（多功能腳）設定，範例預設 LED、RS485 DIR、UART 與其他 I/O 腳位。

開發時常見流程：先在 PC 上編譯與模擬（或直接使用開發板），再透過 pyOCD 將韌體燒入並使用 GDB 或序列埠觀察輸出（範例中使用 retarget/printf 到 UART）。

## 編輯或新增範例檔案

若要加入新的 source/header 到某個 sample（例如在 `Smart_Box` 中加入新驅動）：

1. 將檔案放到適當的子目錄（例如 `SampleCode/Smart_Box/` 或 `Library/StdDriver/src`）。
2. 更新該專案的 cproject.yml（位於 `SampleCode/<Project>/VSCode/` 中的 `.cproject.yml` 或 `*.csolution.yml`）下的 `groups:` 與 `add-path:`，以便 cbuild 能找到新檔案。
	- 範例 repo 中的慣例已在 `SampleCode/Smart_Box/VSCode/*.csolution.yml` 中設定好編譯器條件（`for-compiler`）與連結腳本。
3. 遵循 Nuvoton 的初始化順序：先啟用模組時鐘 → 選擇時鐘來源 → 設定 MFP pinmux → 呼叫驅動初始化。

## 常見除錯提示

- 若無法連接板子，先檢查 pyOCD 與 CMSIS-DAP probe 是否可見（`pyocd list`）。
- 若 printf 沒有輸出，請確認 `SYS_Init()` 的 UART clock 與多工腳設定正確，以及 retarget 設定已啟用。
- 小心不要同時包含多個啟動檔（Startup）於同一個 toolchain 的建置中（GCC vs AC6）。



## 參考與授權

- Nuvoton M480 系列文件與 CMSIS-Toolbox 使用說明請參考官方文件。
- 本倉庫範例多為示例程式，請在商業產品或大量部署前自行檢查相容性與授權。

---



