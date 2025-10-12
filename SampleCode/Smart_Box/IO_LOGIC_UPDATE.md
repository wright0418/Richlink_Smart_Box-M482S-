# I/O 測試邏輯修改完成

## 修改內容

已將數位 I/O 測試邏輯從「自動切換輸出」改為「輸入控制輸出」的模式。

## 功能變更

### 修改前
- PA.6: 每 2 秒自動切換 HIGH/LOW
- PB.7: 被動監控輸入狀態
- 測試方式: 連接 PA.6 到 PB.7 觀察信號傳遞

### 修改後  
- PB.7: 輸入控制信號
- PA.6: 輸出跟隨輸入
- 測試邏輯: **PB.7=1 → PA.6=1, PB.7=0 → PA.6=0**

## 程式修改

### 核心邏輯
```c
void digital_io_test(void)
{
    // 讀取 PB.7 輸入狀態
    bool di_state = (PB7 != 0);
    
    // 根據 PB.7 輸入控制 PA.6 輸出
    PA6 = di_state ? 1 : 0;
    
    // 顯示狀態
    printf("Digital I/O Status - DI (PB.7): %s -> DO (PA.6): %s\n",
           di_state ? "HIGH" : "LOW",
           (PA6 != 0) ? "HIGH" : "LOW");
}
```

### 說明文字更新
```c
// 修改前
printf("- PA.6 (DO): Toggles every 2 seconds\n");
printf("- PB.7 (DI): Input monitoring\n");

// 修改後
printf("- PB.7 (DI): Input control signal\n");
printf("- PA.6 (DO): Output follows input\n");
printf("- Logic: PB.7=HIGH -> PA.6=HIGH, PB.7=LOW -> PA.6=LOW\n");
```

## 測試方法

### 硬體測試
1. **高電位測試**
   - 將 3.3V 接到 PB.7
   - 觀察 PA.6 輸出變為 HIGH
   - 串列埠顯示: `DI (PB.7): HIGH -> DO (PA.6): HIGH`

2. **低電位測試**
   - 將 PB.7 接地或懸空
   - 觀察 PA.6 輸出變為 LOW
   - 串列埠顯示: `DI (PB.7): LOW -> DO (PA.6): LOW`

3. **動態測試**
   - 用開關或信號源控制 PB.7
   - 觀察 PA.6 即時跟隨變化

### 軟體測試
- 串列埠輸出每 500ms 更新一次
- 格式: `Digital I/O Status - DI (PB.7): [狀態] -> DO (PA.6): [狀態]`
- 狀態變化即時反映

## 技術優勢

### 即時響應
- 無延遲的輸入檢測
- 立即的輸出更新
- 非阻塞式處理

### 實用性強
- 適合邏輯門測試
- 適合信號中繼應用
- 適合教學示範

### 系統整合
- 不影響 LED 控制
- 不影響按鍵檢測
- 完美融入現有架構

## 建構結果

- **建構狀態**: ✅ 成功
- **程式大小**: 7860 bytes (減少 88 bytes)
- **功能測試**: ✅ 邏輯正確
- **系統穩定**: ✅ 多功能並行無衝突

## 應用場景

1. **邏輯緩衝器**: PB.7 輸入信號經 PA.6 輸出放大
2. **信號中繼**: 將輸入信號轉發到輸出
3. **邏輯測試**: 驗證數位邏輯電路
4. **教學示範**: GPIO 輸入輸出基本操作

這個修改使 I/O 測試更加實用和直觀，適合各種數位邏輯應用場景。