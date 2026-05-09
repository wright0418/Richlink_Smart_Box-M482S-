"""Compatibility thin wrapper reworked for memory constrained environments.

This module no longer contains all modes by default. Import the small
core class and register only the modes you need. Example:

    from Module.epyRGB_core import RGBModeDisplay
    from Module.epyRGB_modes_basic import MODES as BASIC

    disp = RGBModeDisplay(precompute_palette=False)
    disp.register_modes(BASIC)
    disp.set_mode('rainbow')

Keeping modes in separate modules lets MicroPython users import only a
few functions and conserve RAM.
"""

from .epyRGB_core import RGBModeDisplay

__all__ = ['RGBModeDisplay']

