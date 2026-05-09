# Web BLE 創意打地鼠 / M482 測試頁

本資料夾保留早期打地鼠原型，同時提供目前實務使用的 **單一 Web BLE 測試 UI**（`index.html`）供 M482 連線與封包驗證。

## 目前用途

- `index.html`
	- 單一裝置 Web BLE 連線（Slot 1）
	- 自訂 8x8 樣板與顏色發送
	- 即時顯示 M482 通知事件（RX bytes）
- 多裝置遊戲模式功能仍保留，用於互動展示與回歸測試。

## 協定（本頁面）

- Header: `0xAA`
- Footer: `0x55`
- 服務/特徵（M482）
	- Service: `524cacc0-3c17-d293-8e48-14fe2e4da212`
	- TX (write): `0000d002-0000-1000-8000-00805f9b34fb`
	- RX (notify): `0000d001-0000-1000-8000-00805f9b34fb`

## 使用方式

請以 `localhost` 或 `HTTPS` 開啟（Web Bluetooth 需要 secure context，不建議 `file://`）。

詳細操作請看：`SINGLE_WEB_BLE_TEST_GUIDE.md`

## 內容說明

- `index.html`：Web BLE 測試與互動頁面
- `SINGLE_WEB_BLE_TEST_GUIDE.md`：單一 BLE 測試流程與注意事項
- `ePy-lite_code/`：舊版硬體端參考（MicroPython）
