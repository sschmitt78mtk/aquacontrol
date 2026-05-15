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
    """Test TemperatureController in simulation mode."""
    settings = get_settings()
    settings.simulateSensor = True

    ctrl = TemperatureController()
    temp = ctrl.read()
    assert 19.0 <= temp <= 35.0, f"Temperature {temp} out of range"
    assert ctrl.current == temp
