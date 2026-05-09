"""epyRGB core engine

提供一個小巧、記憶體友善的 RGB 模式引擎 `RGBModeDisplay`，
設計給 MicroPython / ePy 平台，模式以獨立函式實作以減少綁定開銷。

快速範例：

    import epyRGB_core as rgc
    import epyRGB_modes_basic as modes

    # 建立引擎物件
    disp = rgc.RGBModeDisplay(num_leds=64, brightness=40)

    # 註冊並啟用模式
    disp.register_modes(modes.MODES)
    disp.set_mode('rainbow')
    disp.run()  # blocking 主迴圈

若需要非阻塞控制，可在自訂 loop 中呼叫 `fill_if_due()` 與
`update_if_due()`。
"""

from machine import LED
from utime import sleep_ms, ticks_ms, ticks_diff

# Small, memory-friendly core for RGB modes. Modes are provided as
# standalone functions that accept a single argument `disp` (the display
# instance) and modify `disp.led_buffer`, `disp.phase`, etc.

NUM_LEDS = 64
BRIGHTNESS = 50
DEFAULT_UPDATE_HZ = 30
DEFAULT_WRITE_HZ = 30
DEFAULT_SPEED = 8
MAIN_LOOP_SLEEP_MS = 1


class RGBModeDisplay:
    """Lightweight RGB mode engine. Does NOT register any modes by
    default to keep import memory low. Import mode modules only when
    needed and register them with `register_mode()` or `register_modes()`.
    """

    def __init__(self, num_leds=NUM_LEDS, brightness=BRIGHTNESS,
                 update_hz=DEFAULT_UPDATE_HZ, write_hz=DEFAULT_WRITE_HZ,
                 speed=DEFAULT_SPEED, precompute_palette=False):
        self.num_leds = int(num_leds)
        self.brightness = int(brightness)
        self.update_hz = int(update_hz)
        self.write_hz = int(write_hz)
        self.speed = int(speed)

        # hardware LED object
        self.led = LED(LED.RGB)
        try:
            self.led.lightness(self.brightness)
        except Exception:
            # some ports may not implement lightness
            pass

        # runtime state
        self.led_buffer = [(0, 0, 0) for _ in range(self.num_leds)]
        self.phase = 0
        self.write_request = False

        # mode registry (name -> callable)
        self._modes = {}
        self.mode = None

        # optional palette to save CPU at cost of RAM
        self.palette = None
        if precompute_palette:
            self.palette = [self.wheel(i) for i in range(256)]

        # timing
        self.fill_interval_ms = max(1, int(1000.0 / self.update_hz))
        self.update_interval_ms = max(1, int(1000.0 / self.write_hz))
        self.last_fill_time = ticks_ms()
        self.last_update_time = ticks_ms()

        self.running = False

    # color wheel helper (kept small and pure-Python)
    def wheel(self, pos):
        if pos < 0 or pos > 255:
            return (0, 0, 0)
        if pos < 85:
            r = int(pos * 3)
            g = int(255 - pos * 3)
            b = 0
        elif pos < 170:
            pos -= 85
            r = int(255 - pos * 3)
            g = 0
            b = int(pos * 3)
        else:
            pos -= 170
            r = 0
            g = int(pos * 3)
            b = int(255 - pos * 3)
        return (r, g, b)

    # registration helpers
    def register_mode(self, name, func):
        """Register a mode function. Mode signature: func(disp).
        Use simple functions to avoid binding large objects.
        """
        self._modes[name] = func

    def register_modes(self, mapping):
        for k, v in mapping.items():
            self.register_mode(k, v)

    def set_mode(self, mode):
        """Set current mode by name or callable. If name not found, mode
        will be set to None.
        """
        if callable(mode):
            self.mode = mode
        elif isinstance(mode, str):
            self.mode = self._modes.get(mode, None)
        else:
            self.mode = None

    def fill_if_due(self):
        now = ticks_ms()
        if ticks_diff(now, self.last_fill_time) >= self.fill_interval_ms:
            if self.mode is not None:
                try:
                    # mode is a function that receives this instance
                    self.mode(self)
                except Exception:
                    # swallow mode errors to keep main loop alive
                    pass
            # advance phase
            self.phase = (self.phase + self.speed) % 256
            self.last_fill_time = now

    def update_if_due(self):
        now = ticks_ms()
        if ticks_diff(now, self.last_update_time) >= self.update_interval_ms:
            self.write_request = True
            self.update_leds()
            self.last_update_time = now

    def update_leds(self):
        if not self.write_request:
            return
        try:
            # prefer a tuple for some HW backends
            self.led.rgb_write(tuple(self.led_buffer))
        except Exception:
            try:
                self.led.rgb_write(self.led_buffer)
            except Exception:
                # some ports may require different write forms; ignore
                pass
        self.write_request = False

    def run(self):
        self.running = True
        while self.running:
            self.fill_if_due()
            self.update_if_due()
            sleep_ms(MAIN_LOOP_SLEEP_MS)

    def stop(self):
        self.running = False
