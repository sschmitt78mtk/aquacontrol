"""Tests for MockGPIOController."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.gpio_interface import (
    MockGPIOController, LIGHT_PWMPIN, RELAYLIGHT_PIN, DEVICE_PWMLIGHT, LIGHT_LEVELS
)


def test_mock_pwm():
    """Test setting PWM values."""
    gpio = MockGPIOController()
    gpio.set_pwm(LIGHT_PWMPIN, 512)
    assert gpio.get_pwm(LIGHT_PWMPIN) == 512


def test_mock_pwm_clamping():
    """Test PWM value clamping to 0-1023."""
    gpio = MockGPIOController()
    gpio.set_pwm(LIGHT_PWMPIN, 9999)
    assert gpio.get_pwm(LIGHT_PWMPIN) == 1023
    gpio.set_pwm(LIGHT_PWMPIN, -10)
    assert gpio.get_pwm(LIGHT_PWMPIN) == 0


def test_mock_digital():
    """Test setting and reading digital values."""
    gpio = MockGPIOController()
    gpio.set_digital(RELAYLIGHT_PIN, True)
    assert gpio.get_digital(RELAYLIGHT_PIN) == True
    gpio.set_digital(RELAYLIGHT_PIN, False)
    assert gpio.get_digital(RELAYLIGHT_PIN) == False


def test_mock_cleanup():
    """Test cleanup resets all pins."""
    gpio = MockGPIOController()
    gpio.set_pwm(LIGHT_PWMPIN, 512)
    gpio.set_digital(RELAYLIGHT_PIN, True)
    gpio.cleanup()
    assert gpio.get_pwm(LIGHT_PWMPIN) == 0
    assert gpio.get_digital(RELAYLIGHT_PIN) == False


def test_mock_log():
    """Test logging of actions."""
    gpio = MockGPIOController()
    gpio.set_pwm(LIGHT_PWMPIN, 100)
    gpio.set_digital(RELAYLIGHT_PIN, True)
    log = gpio.get_log()
    assert len(log) == 2
    assert "set_pwm" in log[0]
    assert "set_digital" in log[1]


def test_light_levels():
    """Test light levels array."""
    assert len(LIGHT_LEVELS) == 7
    assert LIGHT_LEVELS[0] == 0
    assert LIGHT_LEVELS[6] == 1023
