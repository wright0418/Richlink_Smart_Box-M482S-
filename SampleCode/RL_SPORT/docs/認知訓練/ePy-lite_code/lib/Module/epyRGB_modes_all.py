"""All modes collection.

Modes are lightweight functions taking a single `disp` argument. State
that would consume memory is allocated lazily on the `disp` instance the
first time the mode runs. This keeps import cost small until a mode is
actually used.
"""

import urandom as random


def _get_color(disp, idx):
    if getattr(disp, 'palette', None) is not None:
        return disp.palette[idx & 255]
    return disp.wheel(idx & 255)


def _ensure_attr(disp, name, factory):
    if not hasattr(disp, name):
        setattr(disp, name, factory())
    return getattr(disp, name)


def rainbow(disp):
    for i in range(disp.num_leds):
        rc_index = (i * 255 // max(1, disp.num_leds)) + disp.phase
        disp.led_buffer[i] = _get_color(disp, rc_index)


def solid(disp, color=None):
    c = color if color is not None else _get_color(disp, disp.phase)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c


def primary_cycle(disp):
    idx = (disp.phase // 85) % 3
    if idx == 0:
        c = (255, 0, 0)
    elif idx == 1:
        c = (0, 255, 0)
    else:
        c = (0, 0, 255)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c


def random_flash(disp):
    c = (random.getrandbits(8), random.getrandbits(8), random.getrandbits(8))
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c


def chase(disp):
    # use lazy chase_pos
    chase_pos = _ensure_attr(disp, 'chase_pos', lambda: 0)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (0, 0, 0)
    pos = chase_pos % disp.num_leds
    disp.led_buffer[pos] = _get_color(disp, disp.phase)
    disp.chase_pos = (pos + 1) % disp.num_leds


def breathing(disp):
    base = _get_color(disp, disp.phase)
    t = (disp.phase % 256) / 255.0
    intensity = 1.0 - abs(2.0 * t - 1.0)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (
            int(base[0] * intensity), int(base[1] * intensity), int(base[2] * intensity))


def color_wipe(disp):
    n = (disp.phase * disp.num_leds) // 256
    c = _get_color(disp, disp.phase)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c if i <= n else (0, 0, 0)


def gradient(disp):
    for i in range(disp.num_leds):
        disp.led_buffer[i] = _get_color(disp, (i * 255 // max(1, disp.num_leds)) + disp.phase)


def theater_chase(disp):
    for i in range(disp.num_leds):
        if (i + disp.phase) % 3 == 0:
            disp.led_buffer[i] = _get_color(disp, disp.phase)
        else:
            disp.led_buffer[i] = (0, 0, 0)


def twinkle(disp):
    # per-pixel counters/colors lazily created
    twinkle_counters = _ensure_attr(disp, 'twinkle_counters', lambda: [0] * disp.num_leds)
    twinkle_colors = _ensure_attr(disp, 'twinkle_colors', lambda: [(0, 0, 0)] * disp.num_leds)
    for i in range(disp.num_leds):
        if twinkle_counters[i] > 0:
            twinkle_counters[i] -= 1
            disp.led_buffer[i] = twinkle_colors[i]
        else:
            if (random.getrandbits(8) % 50) == 0:
                col = _get_color(disp, random.getrandbits(8))
                twinkle_colors[i] = col
                twinkle_counters[i] = (random.getrandbits(5) % 20) + 5
                disp.led_buffer[i] = col
            else:
                disp.led_buffer[i] = (0, 0, 0)


def sparkle(disp):
    sparkle_counters = _ensure_attr(disp, 'sparkle_counters', lambda: [0] * disp.num_leds)
    sparkle_colors = _ensure_attr(disp, 'sparkle_colors', lambda: [(0, 0, 0)] * disp.num_leds)
    for i in range(disp.num_leds):
        if sparkle_counters[i] > 0:
            sparkle_counters[i] -= 1
            disp.led_buffer[i] = sparkle_colors[i]
        else:
            if random.getrandbits(8) % 60 == 0:
                c = _get_color(disp, random.getrandbits(8))
                sparkle_colors[i] = c
                sparkle_counters[i] = (random.getrandbits(5) % 10) + 3
                disp.led_buffer[i] = c
            else:
                disp.led_buffer[i] = (0, 0, 0)


def meteor(disp):
    _ensure_attr(disp, 'meteor_pos', lambda: 0)
    _ensure_attr(disp, 'meteor_size', lambda: max(3, disp.num_leds // 8))
    # fade
    for i in range(disp.num_leds):
        v = disp.led_buffer[i]
        disp.led_buffer[i] = (int(v[0] * 0.3), int(v[1] * 0.3), int(v[2] * 0.3))
    pos = disp.meteor_pos % disp.num_leds
    for t in range(disp.meteor_size):
        idx = (pos - t) % disp.num_leds
        intensity = max(0.0, 1.0 - (t / max(1, disp.meteor_size)))
        w = _get_color(disp, disp.phase)
        disp.led_buffer[idx] = (int(w[0] * intensity), int(w[1] * intensity), int(w[2] * intensity))
    disp.meteor_pos = (pos + 1) % disp.num_leds


def strobe(disp):
    strobe_on = _ensure_attr(disp, 'strobe_on', lambda: False)
    if random.getrandbits(8) % 10 == 0:
        disp.strobe_on = not disp.strobe_on
    c = _get_color(disp, disp.phase) if disp.strobe_on else (0, 0, 0)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = c


def scanner(disp):
    _ensure_attr(disp, 'scanner_pos', lambda: 0)
    _ensure_attr(disp, 'scanner_dir', lambda: 1)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (0, 0, 0)
    p = disp.scanner_pos
    disp.led_buffer[p] = _get_color(disp, disp.phase)
    p += disp.scanner_dir
    if p >= disp.num_leds or p < 0:
        disp.scanner_dir = -disp.scanner_dir
        p = max(0, min(p, disp.num_leds - 1))
    disp.scanner_pos = p


def confetti(disp):
    sparkle_counters = _ensure_attr(disp, 'sparkle_counters', lambda: [0] * disp.num_leds)
    sparkle_colors = _ensure_attr(disp, 'sparkle_colors', lambda: [(0, 0, 0)] * disp.num_leds)
    confetti_decay = _ensure_attr(disp, 'confetti_decay', lambda: 20)
    for i in range(disp.num_leds):
        if sparkle_counters[i] > 0:
            sparkle_counters[i] -= 1
            disp.led_buffer[i] = sparkle_colors[i]
        else:
            if random.getrandbits(8) % 30 == 0:
                c = _get_color(disp, random.getrandbits(8))
                sparkle_colors[i] = c
                sparkle_counters[i] = confetti_decay
                disp.led_buffer[i] = c
            else:
                disp.led_buffer[i] = (0, 0, 0)


def fire(disp):
    fire_heat = _ensure_attr(disp, 'fire_heat', lambda: [0] * disp.num_leds)
    for i in range(disp.num_leds):
        cooldown = random.getrandbits(5) % 3
        fire_heat[i] = max(0, fire_heat[i] - cooldown)
    if random.getrandbits(8) % 2 == 0:
        idx = random.getrandbits(8) % disp.num_leds
        fire_heat[idx] = min(255, fire_heat[idx] + random.getrandbits(6))
    for i in range(disp.num_leds):
        h = fire_heat[i]
        r = min(255, h)
        g = min(255, int(h * 0.6))
        b = max(0, int(h * 0.2) - 10)
        disp.led_buffer[i] = (r, g, b)


def rainbow_cycle(disp):
    color_chase_offset = _ensure_attr(disp, 'color_chase_offset', lambda: 0)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = _get_color(disp, (i * 256 // disp.num_leds) + disp.color_chase_offset)
    disp.color_chase_offset = (disp.color_chase_offset + disp.speed) % 256


def color_chase(disp):
    color_chase_offset = _ensure_attr(disp, 'color_chase_offset', lambda: 0)
    seg = max(1, disp.num_leds // 8)
    for i in range(disp.num_leds):
        if ((i + disp.color_chase_offset) // seg) % 2 == 0:
            disp.led_buffer[i] = _get_color(disp, disp.phase)
        else:
            disp.led_buffer[i] = (0, 0, 0)
    disp.color_chase_offset = (disp.color_chase_offset + 1) % 256


def pulse(disp):
    t = (disp.phase % 256) / 255.0
    intensity = 0.5 + 0.5 * (1.0 - abs(2.0 * t - 1.0))
    c = _get_color(disp, disp.phase)
    for i in range(disp.num_leds):
        disp.led_buffer[i] = (int(c[0] * intensity), int(c[1] * intensity), int(c[2] * intensity))


def gradient_shift(disp):
    for i in range(disp.num_leds):
        disp.led_buffer[i] = _get_color(disp, (i * 255 // max(1, disp.num_leds)) + disp.phase)


# export mapping
MODES = {
    'rainbow': rainbow,
    'solid': solid,
    'primary_cycle': primary_cycle,
    'random_flash': random_flash,
    'chase': chase,
    'breathing': breathing,
    'color_wipe': color_wipe,
    'gradient': gradient,
    'theater_chase': theater_chase,
    'twinkle': twinkle,
    'sparkle': sparkle,
    'meteor': meteor,
    'strobe': strobe,
    'scanner': scanner,
    'confetti': confetti,
    'fire': fire,
    'rainbow_cycle': rainbow_cycle,
    'color_chase': color_chase,
    'pulse': pulse,
    'gradient_shift': gradient_shift,
}
