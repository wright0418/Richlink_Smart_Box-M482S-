# Single Web BLE 測試指南（M482）

本文件說明如何使用 `index.html` 的「單一裝置測試」區塊進行快速驗證。

## 功能範圍

- 連接單一 BLE 裝置（固定 Slot 1）
- 設定樣板（Pattern）、顏色（Color）、目標旗標（Tag）
- 送出 13-byte LED frame 封包
- 顯示 M482 回傳的通知事件（hex bytes）

## 前置條件

- 瀏覽器支援 Web Bluetooth（建議 Chrome / Edge）
- 透過 `localhost` 或 `https` 開啟頁面
- M482 韌體已開啟對應 BLE Service/Characteristic

## 操作步驟

1. 開啟 `index.html`
2. 在「單一裝置測試」點擊「連接單一裝置」
3. 在系統裝置選單選擇目標 BLE 裝置
4. 連線成功後，選擇 Pattern / Color / Tag，按 `Send`
5. 觀察：
   - 裝置 LED 是否符合預期
   - 「接收事件 (M482 → Web)」是否出現通知資料

## 封包格式（LED Frame）

- 總長度：13 bytes
- 結構：
  - Byte 0: Header `0xAA`
  - Byte 1: Color
  - Byte 2: Target Tag (`0x01` 或 `0x00`)
  - Byte 3~10: 8x8 Rows
  - Byte 11: XOR checksum（Byte1~Byte10）
  - Byte 12: Footer `0x55`

## 程式架構重點

- `RealBLEAdapter`：
  - 封裝 connect/write/notify
  - 支援 `subscribeRx(slotId, callback)` 訂閱 RX 事件
- `initSingleBleUI()`：
  - 綁定單一測試 UI 的 connect/send 事件
  - 將 RX 事件顯示在 `eventArea`

## 常見問題

- 看不到裝置：
  - 確認藍牙已開啟、裝置可被掃描
  - 改用 `acceptAllDevices` 掃描（目前已內建）
- 可連線但無通知：
  - 確認韌體有啟用 notify
  - 檢查 RX UUID 是否為 `D001`
- 送出失敗：
  - 確認 TX UUID 可寫入（`D002`）
  - 檢查裝置是否斷線
