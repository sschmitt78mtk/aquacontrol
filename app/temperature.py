"""Temperature module - port of ESP8266 temperature reading + conversion."""

import random
import glob
from app.config import get_settings


def temp_float2int(absolutetemp: float) -> int:
    """Convert absolute temperature to int8 deviation from 25°C * 10.
    E.g., 25.0 -> 0, 26.5 -> 15, 23.0 -> -20"""
    return int((absolutetemp - 25.0) * 10)


def temp_int2float(temp_deviation: int) -> float:
    """Convert deviation from 25°C * 10 back to absolute temperature.
    E.g., 0 -> 25.0, 15 -> 26.5, -20 -> 23.0"""
    return 25.0 + (temp_deviation / 10.0)


class TemperatureController:
    """Handles temperature reading from DS18B20 sensor or simulation."""

    def __init__(self):
        self._last_temp = 25.0
        self._base_temp = 25.0
        self._call_counter = 0

    def read(self) -> float:
        """Read current temperature. Returns float in °C."""
        settings = get_settings()
        if settings.simulateSensor:
            self._call_counter += 1

            # Every 10 calls, update the base temperature with a larger drift
            if self._call_counter % 10 == 1:
                self._base_temp += random.uniform(-0.5, 0.5)
                self._base_temp = max(22.0, min(30.0, self._base_temp))

            # Every call: add small jitter (±0.1°C) to the base
            self._last_temp = round(self._base_temp + random.uniform(-0.1, 0.1), 1)
        else:
            self._last_temp = self._read_ds18b20()
        return self._last_temp

    def _read_ds18b20(self) -> float:
        """Read temperature from DS18B20 on RPi via w1 interface."""
        try:
            base_dirs = glob.glob("/sys/bus/w1/devices/28-*/w1_slave")
            if not base_dirs:
                return self._last_temp  # fallback to last value
            with open(base_dirs[0], "r") as f:
                data = f.read()
            if "YES" not in data:
                return self._last_temp  # CRC error
            temp_str = data.split("t=")[-1]
            return round(float(temp_str) / 1000.0, 1)
        except (FileNotFoundError, IndexError, ValueError, OSError):
            return self._last_temp  # sensor error, return last valid

    @property
    def current(self) -> float:
        return self._last_temp
