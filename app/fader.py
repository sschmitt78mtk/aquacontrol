"""PWM Fader module - port of ESP8266 backlight_fader.cpp.

Provides smooth PWM transitions using time.monotonic().
10-bit PWM resolution (0-1023)."""

import time


class PWMFader:
    """Smooth PWM fader - mimics the ESP8266 backlight class."""

    def __init__(self):
        self.brightness: int = 0
        self.brightness_last: int = 0
        self.busy: bool = False
        self._init()

    def _init(self):
        """Reset internal state (called from __init__ and init())."""
        self.brightness = 0
        self.brightness_last = 0
        self.busy = False
        self._brightness_target: int = 0
        self._brightness_started: int = 0
        self._brightness_started_ms: float = 0.0
        self._xend_ms: float = 0.0
        self._xduration_ms: int = 0
        self._xduration_ms_last: int = 0
        self._next_update_ms: float = 0.0
        self._next_update_stepsize: float = 0.0

    def init(self):
        self._init()

    def fade(self, target_brightness: int, duration_ms: int = 2000):
        """Start fading to target_brightness over duration_ms."""
        if target_brightness == self.brightness:
            self.busy = False
            self._brightness_target = target_brightness
            self._xduration_ms_last = duration_ms
            return

        if (self._brightness_target != target_brightness or
                self._xduration_ms_last != duration_ms):
            self._brightness_target = target_brightness
            self._xduration_ms_last = duration_ms
            self._brightness_started = self.brightness
            now = time.monotonic()
            self._brightness_started_ms = now
            self._xend_ms = now + (duration_ms / 1000.0)
            self._xduration_ms = duration_ms

            # Calculate step size
            if duration_ms > 0:
                self._next_update_stepsize = duration_ms / 1023.0
            else:
                self._next_update_stepsize = 0
            self._next_update_ms = now + (self._next_update_stepsize / 1000.0)
            self.busy = True

    def upd(self) -> bool:
        """Update brightness based on elapsed time.
        Returns True if brightness value changed."""
        value_changed = False
        now = time.monotonic()

        if self.busy and now > self._next_update_ms:
            if now > self._xend_ms:
                self.busy = False
                self.brightness = self._brightness_target
            else:
                self.brightness = self._calc_brightness(
                    self._brightness_started_ms,
                    self._xduration_ms,
                    self._brightness_started,
                    self._brightness_target - self._brightness_started
                )
                self._next_update_ms = now + (self._next_update_stepsize / 1000.0)

            value_changed = self.brightness != self.brightness_last
            self.brightness_last = self.brightness

        return value_changed

    def _calc_brightness(self, dxstart: float, dx: int,
                         dystart: int, dy: int) -> int:
        """Linear interpolation with 64-bit precision to prevent overflow."""
        time_passed_ms = (time.monotonic() - dxstart) * 1000.0
        # Use 64-bit integer arithmetic like the original C++ code
        i64 = int(time_passed_ms) * int(dy)
        if dx > 0:
            i64 //= int(dx)
        i64 += int(dystart)
        if i64 < 0:
            i64 = 0
        if i64 > 1023:
            i64 = 1023
        return i64
