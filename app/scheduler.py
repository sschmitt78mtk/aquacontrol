"""Schedule engine - port of ESP8266 schedule checking logic.

Controls PWM and relay devices based on schedule entries."""

import time
import logging

from app.config import get_settings
from app.gpio_interface import (
    LIGHT_PWMPIN, COOLING_PWMPIN,
    RELAYLIGHT_PIN, RELAYCO2_PIN, RELAYMOON_PIN,
    COOLING_OFF_PIN,
    DEVICE_PWMLIGHT, DEVICE_COOLING, DEVICE_LIGHT,
    DEVICE_CO2, DEVICE_MOON, LIGHT_LEVELS,
    GPIOController,
)
from app.fader import PWMFader
from app.crud import get_crud

logger = logging.getLogger(__name__)


class Scheduler:
    """Scheduler checks schedule entries and controls devices."""

    FISH_SAFE_LIGHT_ON_SECONDS = 10 * 60 * 60
    FISH_SAFE_CYCLE_SECONDS = 24 * 60 * 60
    # Whole-second durations retain the existing one-second application loop.
    # Each item is (moonlight on, duration in seconds): ...---... then pause.
    FISH_SAFE_SOS_PATTERN = (
        (True, 1), (False, 1), (True, 1), (False, 1), (True, 1), (False, 1),
        (True, 3), (False, 1), (True, 3), (False, 1), (True, 3), (False, 1),
        (True, 1), (False, 1), (True, 1), (False, 1), (True, 1), (False, 4),
    )
    FISH_SAFE_SOS_CYCLE_SECONDS = sum(duration for _, duration in FISH_SAFE_SOS_PATTERN)

    def __init__(self, gpio: GPIOController, light_fader: PWMFader, cooling_fader: PWMFader):
        self._gpio = gpio
        self._light_fader = light_fader
        self._cooling_fader = cooling_fader
        self._cooling_shutoff_time: float = 0.0  # monotonic time when cooling should shut off
        self._fish_safe_started_at: float | None = None
        self._fish_safe_light_on: bool | None = None
        self._fish_safe_moon_on: bool | None = None

    def set_relay(self, pin: int, state: bool):
        """Set relay with active-LOW logic (same as ESP8266 setRelay).
        state=True -> relay ON (pin LOW), state=False -> relay OFF (pin HIGH)"""
        self._gpio.set_digital(pin, not state)  # active LOW

    def start_fish_safe_mode(self):
        """Start the clock-invalid emergency mode.

        CO2 and all PWM outputs are forced off. The main light is immediately
        on for ten hours of each monotonic 24-hour cycle; moonlight signals SOS.
        """
        if self._fish_safe_started_at is None:
            self._fish_safe_started_at = time.monotonic()
            logger.warning("[FISH-SAFE] NTP was not synchronized at startup")
        self._cooling_shutoff_time = 0.0
        self._light_fader.init()
        self._cooling_fader.init()
        self._gpio.set_digital(LIGHT_PWMPIN, False)
        self._gpio.set_digital(COOLING_PWMPIN, False)
        self.set_relay(RELAYCO2_PIN, False)
        self.update_fish_safe_mode()

    def stop_fish_safe_mode(self):
        """Stop emergency outputs before normal schedule restoration."""
        self._fish_safe_started_at = None
        self._fish_safe_light_on = None
        self._fish_safe_moon_on = None
        self.set_relay(RELAYLIGHT_PIN, False)
        self.set_relay(RELAYCO2_PIN, False)
        self.set_relay(RELAYMOON_PIN, False)

    def update_fish_safe_mode(self):
        """Apply fish-safe outputs for the current monotonic elapsed time."""
        if self._fish_safe_started_at is None:
            raise RuntimeError("Fish-safe mode has not been started")

        elapsed_seconds = int(time.monotonic() - self._fish_safe_started_at)
        light_on = elapsed_seconds % self.FISH_SAFE_CYCLE_SECONDS < self.FISH_SAFE_LIGHT_ON_SECONDS
        moon_on = self._fish_safe_sos_state(elapsed_seconds)

        # Enforce the safety constraints on every loop pass.
        self._light_fader.init()
        self._cooling_fader.init()
        self._gpio.set_digital(LIGHT_PWMPIN, False)
        self._gpio.set_digital(COOLING_PWMPIN, False)
        self.set_relay(RELAYCO2_PIN, False)

        if light_on != self._fish_safe_light_on:
            self.set_relay(RELAYLIGHT_PIN, light_on)
            self._fish_safe_light_on = light_on
            logger.info("[FISH-SAFE] Main light %s", "ON" if light_on else "OFF")
        if moon_on != self._fish_safe_moon_on:
            self.set_relay(RELAYMOON_PIN, moon_on)
            self._fish_safe_moon_on = moon_on

    def _fish_safe_sos_state(self, elapsed_seconds: int) -> bool:
        """Return the moonlight state for the repeating whole-second SOS signal."""
        position = elapsed_seconds % self.FISH_SAFE_SOS_CYCLE_SECONDS
        for state, duration in self.FISH_SAFE_SOS_PATTERN:
            if position < duration:
                return state
            position -= duration
        return False

    def check_schedule(self, hour: int, minute: int):
        """Check if any schedule entries match the given time and execute them."""
        schedule = get_crud().get_schedule()
        for entry in schedule:
            if entry.hour == hour and entry.minute == minute:
                logger.info(
                    f"[SCHEDULE] Entry: {entry.hour}:{entry.minute:02d} "
                    f"device={entry.device} brightness={entry.brightness} "
                    f"fade={entry.fadeMinutes}min"
                )
                blevel = entry.brightness
                target_brightness = LIGHT_LEVELS[blevel] if blevel < len(LIGHT_LEVELS) else 0

                if entry.device == DEVICE_PWMLIGHT:
                    if blevel < 7:
                        fade_ms = entry.fadeMinutes * 60000 + 500
                        self._light_fader.fade(target_brightness, fade_ms)
                        logger.info(f"[SCHEDULE] DEVICE_PWMLIGHT Fade: {target_brightness}/1023")

                elif entry.device == DEVICE_COOLING:
                    fade_ms = entry.fadeMinutes * 60000 + 500
                    self._cooling_fader.fade(target_brightness, fade_ms)
                    logger.info(f"[SCHEDULE] DEVICE_COOLING Fade: {target_brightness}/1023")

                elif entry.device == DEVICE_LIGHT:
                    logger.info(f"[SCHEDULE] DEVICE_LIGHT: {'ON' if blevel > 0 else 'OFF'}")
                    self.set_relay(RELAYLIGHT_PIN, blevel > 0)

                elif entry.device == DEVICE_CO2:
                    logger.info(f"[SCHEDULE] DEVICE_CO2: {'ON' if blevel > 0 else 'OFF'}")
                    self.set_relay(RELAYCO2_PIN, blevel > 0)

                elif entry.device == DEVICE_MOON:
                    logger.info(f"[SCHEDULE] DEVICE_MOON: {'ON' if blevel > 0 else 'OFF'}")
                    self.set_relay(RELAYMOON_PIN, blevel > 0)

                else:
                    logger.warning(f"[SCHEDULE] Unknown device: {entry.device}")

    def set_outputs_according_to_schedule(self, hour: int, minute: int):
        """Restore device states on startup by replaying all entries up to current time."""
        logger.info(f"[SCHEDULE] Restoring outputs up to {hour:02d}:{minute:02d}")
        for h in range(hour + 1):
            max_min = minute if h == hour else 59
            for m in range(max_min + 1):
                self.check_schedule(h, m)

    def check_max_cooling_time(self):
        """Auto-shutoff cooling after maxcooling_mins or low water level.
        Port of ESP8266 checkmaxcoolingTime() which also checks COOLING_OFF_PIN."""
        settings = get_settings()
        if self._cooling_fader.brightness > 0:
            # Check water level sensor (COOLING_OFF_PIN LOW = low water)
            low_water = not self._gpio.get_digital(COOLING_OFF_PIN)
            if self._cooling_shutoff_time == 0.0:
                self._cooling_shutoff_time = time.monotonic() + settings.maxcooling_mins * 60
                logger.info("[SCHEDULE] Cooling ON - timer started")
            elif time.monotonic() > self._cooling_shutoff_time or low_water:
                reason = "low water level" if low_water else "max cooling time reached"
                logger.info(f"[SCHEDULE] Cooling auto-shutoff: {reason}")
                self._cooling_fader.fade(0, 5)
                self._cooling_fader.upd()
                self._gpio.set_digital(COOLING_PWMPIN, False)  # PWM pin: LOW = off
                self._cooling_shutoff_time = 0.0
        else:
            self._cooling_shutoff_time = 0.0

    def off_all(self):
        """Turn everything off (scheduled at 23:00).
        PWM pins use standard logic (LOW=off). Relays use active-LOW."""
        logger.info("[SCHEDULE] off_all()")
        if self._light_fader.brightness != 0:
            self._light_fader.fade(0, 5)
            self._light_fader.upd()
            self._gpio.set_digital(LIGHT_PWMPIN, False)  # PWM pin: LOW = off

        if self._cooling_fader.brightness != 0:
            self._cooling_fader.fade(0, 5)
            self._cooling_fader.upd()
            self._gpio.set_digital(COOLING_PWMPIN, False)  # PWM pin: LOW = off

        self.set_relay(RELAYLIGHT_PIN, False)  # Relay OFF (active LOW -> HIGH)
        self.set_relay(RELAYCO2_PIN, False)    # Relay OFF

    def update_pwm_outputs(self):
        """Called every second to apply fader updates to GPIO."""
        gpio = self._gpio
        if self._light_fader.upd():
            self._apply_pwm(LIGHT_PWMPIN, self._light_fader.brightness)
        if self._cooling_fader.upd():
            self._apply_pwm(COOLING_PWMPIN, self._cooling_fader.brightness)

    def _apply_pwm(self, pin: int, value: int):
        """Apply PWM value to GPIO pin, handling 0/1023 edge cases.
        PWM pins use standard logic (LOW=off, HIGH=on), NOT active-LOW like relays.
        This matches the ESP8266 where setRelay for PWM pins uses digitalWrite(pin, LOW/HIGH)
        without the active-LOW inversion used for relay pins."""
        if value == 0:
            self._gpio.set_digital(pin, False)  # LOW = off (standard logic)
        elif value >= 1023:
            self._gpio.set_digital(pin, True)   # HIGH = on (standard logic)
        else:
            self._gpio.set_pwm(pin, value)
