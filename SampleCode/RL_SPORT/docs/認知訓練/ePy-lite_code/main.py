# 打地鼠裝置 接收 ble 傳來的指令並顯示 RGB LED 顏色 與 8x8 的點陣圖
"""
Protocol (updated):
- Packet length: 13 bytes
- [0] Header 0xAA
- [1] Color (0x00..0x06)
- [2] Target Tag (0x01 target / 0x00 non-target)
- [3..10] 8 bytes bitmap (row0..row7), each byte MSB->LSB => col0..col7
- [11] Checksum = XOR of bytes [1..10] (includes Tag)
-- [12] Footer 0x55

Note: the implementation currently checks for footer byte 0x55 (was
previously documented as 0xFF) — keep protocol and code consistent.

Behavior:
- Receive packet via BLE (GattUART), validate and display on 8x8 RGB matrix
- When Key A is pressed, reply over BLE with single byte 0x01 
"""

from machine import UART, Switch
from utime import sleep_ms, ticks_ms
import gc
from Module.BleGatt import GattUART
from Module.epyRGB_core import RGBModeDisplay

# color mapping (protocol -> (R,G,B))
_COLORS = {
	0x00: (0, 0, 0),    # off
	0x01: (255, 0, 0),  # red
	0x02: (0, 255, 0),  # green
	0x03: (0, 0, 255),  # blue
	0x04: (255, 255, 0),# yellow
	0x05: (128, 0, 128),# purple
	0x06: (255, 255, 255),# white
}

_PACKET_LEN = 13
_HEADER_BYTE = 0xAA
_FOOTER_BYTE = 0x55
_MAX_DBG_BYTES = 52
_RX_CACHE_LIMIT = 256  # avoid unlimited growth if peer floods data


class WhacAMoleDevice:
	def __init__(self, uart_id=1, debug=False):
		self.uart = UART(uart_id)
		self.ble = GattUART(self.uart, debug=debug)
		# store debug flag for verbose logging
		self.debug = bool(debug)
		# use new RGBModeDisplay core API: explicitly set num_leds and lower brightness to 20%
		self.rgb = RGBModeDisplay(num_leds=64, brightness=20, precompute_palette=False)
		# keep current bitmap (list of 64 ints 0/1) and color
		self.bitmap = [0] * 64
		self.color = (0, 0, 0)
		self.is_target = False
		self._pending_resp = None
		self._pending_ts = None
		self._rx_cache = bytearray()
		# GC/monitoring
		self._gc_interval_ms = 2000
		self._last_gc_time = ticks_ms()

		# Key A switch (polling, avoid ISR callback to prevent TypeError/object not callable)
		try:
			self.key = Switch('keya')
		except Exception:
			self.key = None
		self._key_prev = 1  # assume unpressed high

	def _debug(self, template, *args):
		if self.debug:
			print(template.format(ticks_ms(), *args))

	def _on_key(self, *args):
		response_byte = 0x01
		self._debug('KEY DBG {}: queuing response {} (is_target={})', response_byte, self.is_target)
		self._pending_resp = bytes([response_byte])
		self._pending_ts = ticks_ms()

	def _do_send_pending(self):
		pending = self._pending_resp
		if not pending:
			return
		try:
			self.ble.send(pending)
			self._debug('KEY DBG {}: sent {}', '0x%02X' % pending[0])
		except Exception as e:
			self._debug('KEY DBG {}: send error: {}', e)
		finally:
			self._pending_resp = None
			self._pending_ts = None

	def _poll_key(self):
		# Poll key for falling edge to queue response
		if self.key is None:
			return
		try:
			val = self.key.value() if hasattr(self.key, 'value') else (self.key() if callable(self.key) else None)
			if val is None:
				return
			prev = self._key_prev
			self._key_prev = val
			if prev == 1 and val == 0:
				self._on_key()
		except Exception as e:
			self._debug('KEY DBG {}: _poll_key error: {}', e)

	def _parse_packets(self):
		if not self._rx_cache:
			return
		buf = memoryview(self._rx_cache)
		i = 0
		L = len(buf)
		if self.debug:
			hex_view = ' '.join('{:02X}'.format(b) for b in buf[:_MAX_DBG_BYTES])
			if L > _MAX_DBG_BYTES:
				hex_view += ' ...'
			self._debug('BLE DBG PARSE {}: len={} hex={}', L, hex_view)
		while i + _PACKET_LEN <= L:
			if buf[i] != _HEADER_BYTE:
				i += 1
				continue
			if buf[i + _PACKET_LEN - 1] != _FOOTER_BYTE:
				self._debug('BLE DBG BAD FOOTER {}: idx={} footer={:02X}', i, buf[i + _PACKET_LEN - 1])
				i += 1
				continue
			chk = 0
			for b in buf[i + 1:i + 11]:
				chk ^= b
			if chk != buf[i + 11]:
				self._debug('BLE DBG CHK MISMATCH {}: idx={} calc={:02X} pkt={:02X}', i, chk, buf[i + 11])
				i += 1
				continue
			color_byte = buf[i + 1]
			tag_byte = buf[i + 2]
			bitmap_bytes = buf[i + 3:i + 11]
			self.is_target = (tag_byte == 0x01)
			if self.debug:
				bmp_hex = ' '.join('{:02X}'.format(b) for b in bitmap_bytes)
				self._debug('BLE DBG PKT {}: idx={} color={:02X} tag={:02X} bitmap={}', i, color_byte, tag_byte, bmp_hex)
			self._apply_bitmap(bitmap_bytes, color_byte)
			i += _PACKET_LEN
		if i:
			try:
				del self._rx_cache[:i]
			except Exception:
				self._rx_cache = bytearray(buf[i:])
		# 若 buffer 過大（可能因為雜訊或無效資料），僅保留最新資料
		if len(self._rx_cache) > _RX_CACHE_LIMIT:
			del self._rx_cache[:-_RX_CACHE_LIMIT]

	def _apply_bitmap(self, bitmap_bytes, color_byte):
		self.color = _COLORS.get(color_byte, (255, 255, 255))
		# Check and collect GC if memory low before filling LED buffer
		if gc.mem_free() < 4096:
			self._debug('MEM DBG {}: low memory ({} bytes free), collecting GC', gc.mem_free())
			gc.collect()

		off = (0, 0, 0)
		on = self.color

		def _populate():
			idx = 0
			for row_byte in bitmap_bytes:
				mask = 0x80
				for _ in range(8):
					active = bool(row_byte & mask)
					self.bitmap[idx] = 1 if active else 0
					self.rgb.led_buffer[idx] = on if active else off
					mask >>= 1
					idx += 1

		try:
			_populate()
		except MemoryError:
			self._debug('MEM DBG {}: MemoryError filling leds, attempting GC', gc.mem_free())
			gc.collect()
			try:
				_populate()
			except Exception as e:
				self._debug('MEM DBG {}: failed after gc: {}', e)
				return
		try:
			self.rgb.write_request = True
			self.rgb.update_leds()
		except Exception as e:
			self._debug('MEM DBG {}: hw write failed: {}', e)

	def _append_rx(self, chunk):
		if not chunk:
			return
		try:
			self._rx_cache.extend(chunk)
		except MemoryError:
			# 如果快取不足，清掉前面最舊的資料再塞入，避免全部丟失
			if len(self._rx_cache) > len(chunk):
				del self._rx_cache[:len(chunk)]
				self._rx_cache.extend(chunk)
			else:
				self._rx_cache = bytearray(chunk)
		self._parse_packets()

	def _maybe_collect_gc(self, idle):
		if ticks_ms() - self._last_gc_time < self._gc_interval_ms:
			return
		if not idle:
			return
		free_mem = gc.mem_free()
		if free_mem < 4096:
			self._debug('MEM DBG {}: low memory ({} bytes free), collecting GC', free_mem)
			gc.collect()
			self._debug('MEM DBG {}: after GC, {} bytes free', gc.mem_free())
		self._last_gc_time = ticks_ms()

	def run(self):
		# Main loop: poll BLE, key, and handle pending sends
		while True:
			try:
				self._poll_key()
				self._do_send_pending()
				got_data = False
				while True:
					data = self.ble.recv()
					if not data:
						break
					got_data = True
					self._append_rx(data)
				# 只有在空閒（無新資料）時才做 GC，以避免 UART 快取被硬體覆寫
				self._maybe_collect_gc(idle=not got_data)
			except Exception as e:
				self._debug('RUN ERR {}: {}', e)
			sleep_ms(20)


if __name__ == '__main__':
	dev = WhacAMoleDevice(uart_id=1, debug=False)
	dev.run()


