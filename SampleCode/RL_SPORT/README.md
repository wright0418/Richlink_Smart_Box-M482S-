# 專案配置與應用規格
使用 RL SPORT Board V3 進行開發，以下是專案配置和應用規格的詳細說明：
## 跳繩第三版
  - 跳繩次數只透過 Hall Sensor GPIO 中斷計數 (一圈會觸發兩次)，G-sensor 不參與跳繩偵測
  - 判斷 gsensor 是否靜止不動 60 秒，確認後 關機
  - 一啟動系統 先使用 Power Lock GPIO 鎖住電源開關
  - 使用 LED 表示系統狀態 
  - 使用 Buzzer 通知遊戲狀態
  - Buzzer 使用 Timer 中斷製作方波頻率 發出聲音，關閉時GPIO需要為 Low ,
  - ADC 量測 低電壓 並且 使用快閃Led 通知 低電壓需充電
  - 開機後檢查 BLE NAME 是否已經被改成  ROPE_開頭 , 如果不是改名
  - BLE game 開始進入 DATA MODE ，方便傳輸
  - BLE UART 收到 "get cycle\r\n" 就開始遊戲 ，需要發出 Buzzer 告知使用者開始遊戲
  - BLE UART 收到 "set end\r\n' 遊戲結束，發出Buzzer 長
  - BLE 送出 "send 次數\r\n" 給 APP
  - USB 插入充電自動開機時，PA12 會為 High，進入充電模式：不做 Power Lock、不進入遊戲，停在獨立 while(1)
  