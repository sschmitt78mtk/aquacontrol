"""Tests for temperature conversion and reading."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.temperature import temp_float2int, temp_int2float, TemperatureController
from app.config import get_settings


def test_temp_float2int():
    """Test absolute temp to deviation conversion."""
    assert temp_float2int(25.0) == 0
    assert temp_float2int(26.5) == 15
    assert temp_float2int(23.0) == -20
    assert temp_float2int(35.0) == 100


def test_temp_int2float():
    """Test deviation back to absolute temp."""
    assert temp_int2float(0) == 25.0
    assert temp_int2float(15) == 26.5
    assert temp_int2float(-20) == 23.0
    assert temp_int2float(100) == 35.0


def test_roundtrip():
    """Test roundtrip conversion."""
    import random
    for _ in range(100):
        t = round(random.uniform(10, 40), 1)
        d = temp_float2int(t)
        t2 = temp_int2float(d)
        assert abs(t - t2) < 0.15, f"Roundtrip failed for {t} -> {d} -> {t2}"


def test_temperature_controller_simulated():
    """Test TemperatureController in simulation mode with drift + jitter."""
    settings = get_settings()
    settings.simulateSensor = True

    ctrl = TemperatureController()
    temps = []
    for _ in range(20):
        t = ctrl.read()
        # The .current property should return the value just read
        assert ctrl.current == t, f".current ({ctrl.current}) != read() ({t})"
        temps.append(t)

    # All values should be in a realistic range
    for t in temps:
        assert 21.0 <= t <= 31.0, f"Temperature {t} out of realistic range"

    # Consecutive readings should not jump more than 0.6°C
    # (max drift 0.5 + max jitter 0.1 = 0.6)
    for i in range(1, len(temps)):
        diff = abs(temps[i] - temps[i-1])
        assert diff <= 0.6, f"Temperature jump too large: {temps[i-1]} -> {temps[i]} ({diff}°C)"

    # Over 20 readings, there should be some variation (not all identical)
    assert len(set(temps)) > 1, "Temperatures should vary over multiple readings"
