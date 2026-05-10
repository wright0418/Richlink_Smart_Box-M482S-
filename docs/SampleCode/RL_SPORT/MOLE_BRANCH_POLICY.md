# RL_SPORT Mole 專用分支政策

> 適用分支：`增加-6-axis-sensor-SC7U22`

## 分支定位

此分支為 **Mole（認知訓練/打地鼠）專用開發線**，不作為通用 RL_SPORT 或跳繩版本的回收分支。

- **不合併回 `main`**（除非後續有明確決策變更）
- 文件、測試與命名規則皆以 Mole 應用為優先

## 命名與協議基準

- BLE 裝置名稱前綴固定為：`SPORT_XXXX`
  - `XXXX` 為 MAC 後四碼（大寫）
- Web BLE UUID 基準：
  - Service: `524cacc0-3c17-d293-8e48-14fe2e4da212`
  - TX(write): `0000d002-0000-1000-8000-00805f9b34fb`
  - RX(notify): `0000d001-0000-1000-8000-00805f9b34fb`

## 文件維護規則

- 本分支文件一律使用 Squat mode 專用敘述，不再保留 `ROPE_` 相容語氣。
- 若文件同時描述歷史跳繩行為，需標註為「歷史/參考」，不可與 Squat mode 現況混寫成同級需求。

## 驗證最小集合（Squat mode）

1. `tests/run_tests.ps1` compile-only 或 host-run 通過
2. BLE rename 符合 `SPORT_XXXX`
3. BLE binary packet 路徑（LED / BRIGHTNESS_CMD / HIT_CONFIG）可用
4. Web 單裝置測試頁可連線、可收發（`docs/認知訓練/index.html`）

## 變更提交流程建議

- 每次修改後至少更新一份對應文件（協議或測試指南）
- 若變更封包格式或欄位語意，需同步更新：
  - `SampleCode/RL_SPORT/docs/BLE_PROTOCOL.md`
  - `SampleCode/RL_SPORT/docs/BLE_PACKET_QUICK_REFERENCE.md`
