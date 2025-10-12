# Project
使用 Nuvoton M482 MCU 的 Firmware 專案, 使用標準 C 語言撰寫

# 撰寫注意事項
1. 請遵循 Nuvoton 提供的程式碼範本與架構。
2. 請使用標準 C 語言撰寫程式碼，避免使用非標準的擴展或語法。
3. 請保持程式碼的可讀性與可維護性，適當添加註解以說明程式碼邏輯。
4. MCU 硬體驅動使用HAL。參考Library/StdDriver/src目錄下的程式碼。
5. 驅動的標頭檔案位於 \Library\Device\Nuvoton\M480\Include
6. MCU周邊IP使用方法可以參考 D:\Nuvoton\M480_SmartBox\SampleCode\StdDriver\ 下所有的樣板code
7. 有增加 .h .c 檔案時需要更新專案的配置，確定編譯可以完成

# 硬體設定
- MCU: Nuvoton M482SIDAE (ARM Cortex-M4)
- HXT 12MHz 輸入
- RTC 32.768kHz 輸入 XTAL 輸入
- UART1: 115200, 8, N, 1 (TX: PA.9, RX: PA.8)  接 BLE MESH AT CMD Module
- UART0: 9600, 8, N, 1 (TX: PD.3, RX: PD.2)  接 RS485 RTU DEVICE
- I2C0: (SCL: PB.2, SDA: PB.3) 接 24LC64 EEPROM
- IO
    - PB.3	BLUE LED
    - PB.2	YELLOW LED
    - PB.1	RED LED
    - PB.15	按鍵A
    - PB.14 RS485 DIR
    - PB.7  Digital Input1
    - PA.6  Relay control Pin (High: ON, Low: OFF)
    - PB.5	I2C0_SCL
    - PB.4	I2C0_SDA

    
