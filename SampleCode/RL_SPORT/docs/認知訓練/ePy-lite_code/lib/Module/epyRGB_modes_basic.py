"""A small set of standalone modes. Import only what you need to save RAM.

Each mode is a function taking a single argument `disp` (an
`RGBModeDisplay` instance from `epyRGB_core`). Modes should modify
`disp.led_buffer` in-place and may use `disp.phase`, `disp.wheel()` or
`disp.palette` if available.
"""

def _get_color(disp, idx):
    if disp.palette is not None:
        return disp.palette[idx & 255]
    return disp.wheel(idx & 255)


def rainbow(disp):
    """Rainbow gradient across the strip.

    Uses `disp.phase` to animate. Sets each pixel to a color from the
    wheel/palette based on its position.
    """
    for i in range(disp.num_leds):
        rc_index = (i * 255 // max(1, disp.num_leds)) + disp.phase
        disp.led_buffer[i] = _get_color(disp, rc_index)


def solid(disp, color=None):
    """Fill all LEDs with a single color.

    Args:
        color: optional (r,g,b) tuple. If None, uses `disp.wheel(disp.phase)`.
    """
    # if color is None use wheel at current phase
    c = color if color is not None else _get_color(disp, disp.phase)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c


def chase(disp):
    """Simple single-dot chase.

    Uses `disp.phase` modulo `disp.num_leds` as the lit pixel index.
    """
    # simple single-dot chase using phase as position
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (0, 0, 0)
    pos = disp.phase % disp.num_leds
    disp.led_buffer[pos] = _get_color(disp, disp.phase)


def breathing(disp):
    """Breathing/pulse effect using the wheel color at current phase.

    The brightness follows a triangle wave computed from `disp.phase`.
    """
    base = _get_color(disp, disp.phase)
    t = (disp.phase % 256) / 255.0
    intensity = 1.0 - abs(2.0 * t - 1.0)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (
            int(base[0] * intensity), int(base[1] * intensity), int(base[2] * intensity))


def twinkle(disp, randfunc):
    """Lightweight twinkle effect driven by a random function.

    Note: this mode requires a `randfunc` callable returning an integer
    (e.g. 0..255). Because the core mode interface expects a single
    `disp` argument, use a small wrapper when registering:

        import urandom
        disp.register_mode('twinkle', lambda d: twinkle(d, lambda: urandom.getrandbits(8)))

    randfunc should be inexpensive; avoid heavy RNGs in tight loops.
    """
    # randfunc should be a callable returning an int (0..255)
    # lightweight twinkle: random chance to light each pixel
    for i in range(disp.num_leds):
        if (randfunc() % 50) == 0:
            disp.led_buffer[i] = _get_color(disp, randfunc())
        else:
            disp.led_buffer[i] = (0, 0, 0)


# expose a mapping for easy registration
MODES = {
    'rainbow': rainbow,
    'solid': solid,
    'chase': chase,
    'breathing': breathing,
}
