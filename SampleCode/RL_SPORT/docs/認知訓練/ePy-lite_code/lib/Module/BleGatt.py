"""
RL62M01 GATT UART AT CMD 透傳裝置 Library
專為 ePy-lite 板優化，記憶體友善設計
"""

from utime import sleep_ms, ticks_ms, ticks_diff
import utime

# board = 'ePy-lite'

# 常數定義
_UART_BAUDRATE = 115200
_UART_TIMEOUT = 10
_UART_BUF_SIZE = 256
_CMD_TIMEOUT = 100
_CMD_MODE_WAIT = 150
_DATA_MODE_WAIT = 50
_TIMEOUT_BASE = 500

# AT 指令常數
_AT_CMD_MODE = b'!CCMD@'
_AT_DATA_MODE = b'AT+MODE_DATA\r\n'
_AT_ENABLE_SYSMSG = b'AT+EN_SYSMSG=1'
_AT_CONN_STATE = b'AT+CONN_STATE'
_AT_DISCONNECT = b'AT+DISC'


# 系統訊息常數
_MSG_CMD_MODE = b'SYS-MSG: CMD_MODE OK'
_MSG_DATA_MODE = b'SYS-MSG: DATA_MODE OK'
_MSG_CONNECTED = b'SYS-MSG: CONNECTED OK'
_MSG_DISCONNECTED = b'SYS-MSG: DISCONNECTED OK'
_MSG_OK = b'OK'
_MSG_READY = b'READY OK'


class GattUART:

    def __init__(self, uart, debug=False):
        """
        初始化 RL62M01 模組
        
        參數:
            uart: UART 物件實例
            debug: 若為 True 則列印 TX/RX 訊息
        """
        self._uart = uart
        self._mode = 'CMD'  # CMD 或 DATA
        self._role = ''
        self.is_connected = False
        self._debug = bool(debug)
        
        # 重新初始化 UART
        self._uart.deinit()
        self._uart.init(_UART_BAUDRATE, timeout=_UART_TIMEOUT, 
                       read_buf_len=_UART_BUF_SIZE)
        
        # 初始化模組
        self._init_module()
        
    def _debug_log(self, tag, data):
        """簡單的 debug 列印：tag 為 'TX'/'RX'/'MODE' 等
        若 data 為 bytes/bytearray，優先以可讀文字顯示（不可列印字元以 '.' 取代），
        若內容過於特殊則回退為十六進位顯示。
        """
        if not self._debug:
            return
        try:
            ts = ticks_ms()
            if isinstance(data, (bytes, bytearray)):
                # 嘗試用逐字元方式建立可讀字串（避免使用 decode）
                try:
                    chars = []
                    for b in data:
                        # 可列印 ASCII 範圍 32..126，其他用 '.' 表示
                        if 32 <= b <= 126:
                            chars.append(chr(b))
                        else:
                            chars.append('.')
                    text = ''.join(chars)
                except Exception:
                    text = None
                if text:
                    print("BLE DBG {} {}: {}".format(tag, ts, text))
                else:
                    # 回退為十六進位表示
                    hexs = " ".join("%02X" % b for b in data)
                    print("BLE DBG {} {}: {}".format(tag, ts, hexs))
            else:
                print("BLE DBG {} {}: {}".format(tag, ts, str(data)))
        except Exception:
            # debug 不應影響主流程
            pass

    def _init_module(self):
        """初始化 RL62M01 模組設定"""
        # 確保在 CMD 模式
        self._switch_to_cmd()
        
        # 啟用系統訊息
        resp = self._send_at_cmd(b'AT+EN_SYSMSG=?', 50)
        if b'EN_SYSMSG 0' in resp:
            self._send_at_cmd(_AT_ENABLE_SYSMSG, 50)
        
        # 檢查連線狀態
        resp = self._send_at_cmd(_AT_CONN_STATE, 50)
        self.is_connected = b'CONNECTED OK' in resp
        
        # 清空 UART buffer
        if self._uart.any():
            self._uart.read(self._uart.any())
        self._switch_to_data()
    
    def _send_at_cmd(self, cmd, timeout=_CMD_TIMEOUT):
        """
        送出 AT 指令並等待回應
        
        參數:
            cmd (bytes): AT 指令
            timeout (int): 逾時時間 (ms)
            
        回傳:
            bytes: 回應資料
        """
        self._switch_to_cmd()
        
        # 清空接收 buffer
        if self._uart.any():
            self._uart.read(self._uart.any())
        
        # 送出指令
        # debug log
        try:
            self._debug_log('TX', cmd + b'\r\n')
        except Exception:
            pass
        self._uart.write(cmd)
        self._uart.write(b'\r\n')
        
        # 等待回應
        start = ticks_ms()
        resp = bytearray()
        
        while ticks_diff(ticks_ms(), start) < timeout:
            if self._uart.any():
                chunk = self._uart.read(self._uart.any())
                if chunk:
                    # debug log 接收到的 chunk
                    self._debug_log('RX', chunk)
                    resp.extend(chunk)
        
        # 基本錯誤檢查
        if b'ERROR' in resp:
            raise ValueError("AT command failed with ERROR")
        
        return bytes(resp)
    
    def _switch_mode(self, cmd, confirm_msg, target_mode, wait_time):
        """
        通用模式切換方法
        
        參數:
            cmd (bytes): 切換指令
            confirm_msg (bytes): 確認訊息
            target_mode (str): 目標模式 ('CMD' 或 'DATA')
            wait_time (int): 基礎等待時間 (ms)
        """
        if self._mode == target_mode:
            return

        self._debug_log('TX', cmd)
        self._uart.write(cmd)
        start = ticks_ms()
        timeout = wait_time + _TIMEOUT_BASE
        while ticks_diff(ticks_ms(), start) < timeout:
            if self._uart.any():
                msg = self._uart.read(self._uart.any())
                if msg and confirm_msg in msg:
                    self._mode = target_mode
                    self._debug_log('MODE', bytes(target_mode, 'ascii'))
                    return
        # 若逾時仍假定已切換（保守處理）
        self._mode = target_mode
        self._debug_log('MODE', bytes(target_mode, 'ascii'))
    
    def _switch_to_cmd(self):
        """切換到 CMD 模式"""
        self._switch_mode(_AT_CMD_MODE, _MSG_CMD_MODE, 'CMD', _CMD_MODE_WAIT)

    def _switch_to_data(self):
        """切換到 DATA 模式"""
        self._switch_mode(_AT_DATA_MODE, _MSG_DATA_MODE, 'DATA', _DATA_MODE_WAIT)

    def send(self, data):
        """
        送出資料 (需在連線狀態)
        
        參數:
            data (bytes/bytearray/str): 要送出的資料
        """
        self._switch_to_data()
        
        if isinstance(data, str):
            # 將字串轉為 bytearray（ASCII 假設）
            data = bytearray(data)
        
        # debug log 寫出內容
        self._debug_log('TX', data)
        self._uart.write(data)
    
    def recv(self):
        """
        接收資料並處理系統訊息
        
        回傳:
            bytes: 接收到的資料，若無資料或僅系統訊息則回傳 None
        """
        if not self._uart.any():
            return None
        
        msg = self._uart.read(self._uart.any())
        if not msg:
            return None

        # debug log 原始收到的訊息
        self._debug_log('RX', msg)
        
        if not msg:
            return None
        
        # 檢查系統訊息
        if _MSG_CONNECTED in msg:
            self.is_connected = True
            return None
        elif _MSG_DISCONNECTED in msg:
            self.is_connected = False
            return None
        
        return msg
    
    def disconnect(self):
        """中斷連線（使用 ticks_ms 統一等待檢查）"""
        self._send_at_cmd(_AT_DISCONNECT, 200)

        # 等待中斷或超時（合併為單一 ticks-based 迴圈）
        start = ticks_ms()
        timeout = 1000  # 總等待時間（ms）
        while ticks_diff(ticks_ms(), start) < timeout:
            # 讀取任何可用的訊息來更新 is_connected
            msg = self.recv()
            if not self.is_connected:
                break
    
    def mode(self):
        """取得目前模式"""
        return self._mode

# 簡單的 Class 使用 範例
if __name__ == '__main__':
    from machine import UART
    ble =  GattUART(UART(1),debug=True)
    print("BLE initialized")
    print("Current Mode:", ble.mode())

    while True:
        data = ble.recv()
        if ble.is_connected:
            if data:
                print("Received(bytes):", data)  
                ble.send(data)  # 回傳相同資料

        else:
            print("Disconnected")

        sleep_ms(10)
