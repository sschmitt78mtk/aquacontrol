"""Tests for clock-invalid fish-safe output mode."""

from unittest.mock import patch

from app.fader import PWMFader
from app.gpio_interface import (
    COOLING_PWMPIN, LIGHT_PWMPIN, RELAYCO2_PIN, RELAYLIGHT_PIN, RELAYMOON_PIN,
    MockGPIOController,
)
from app.scheduler import Scheduler


def make_scheduler():
    gpio = MockGPIOController()
    return Scheduler(gpio, PWMFader(), PWMFader()), gpio


def relay_is_on(gpio, pin):
    """Relays are active low."""
    return gpio._digital_values[pin] is False


def test_fish_safe_starts_with_main_light_on_and_co2_and_pwm_off():
    scheduler, gpio = make_scheduler()
    with patch("app.scheduler.time.monotonic", return_value=100.0):
        scheduler.start_fish_safe_mode()

    assert relay_is_on(gpio, RELAYLIGHT_PIN)
    assert not relay_is_on(gpio, RELAYCO2_PIN)
    assert gpio._digital_values[LIGHT_PWMPIN] is False
    assert gpio._digital_values[COOLING_PWMPIN] is False


def test_fish_safe_light_is_on_for_ten_hours_then_off_until_next_cycle():
    scheduler, gpio = make_scheduler()
    with patch("app.scheduler.time.monotonic", return_value=100.0):
        scheduler.start_fish_safe_mode()
    with patch("app.scheduler.time.monotonic", return_value=100.0 + 10 * 60 * 60):
        scheduler.update_fish_safe_mode()
    assert not relay_is_on(gpio, RELAYLIGHT_PIN)

    with patch("app.scheduler.time.monotonic", return_value=100.0 + 24 * 60 * 60):
        scheduler.update_fish_safe_mode()
    assert relay_is_on(gpio, RELAYLIGHT_PIN)


def test_fish_safe_reenforces_co2_and_pwm_off_after_manual_change():
    scheduler, gpio = make_scheduler()
    with patch("app.scheduler.time.monotonic", return_value=100.0):
        scheduler.start_fish_safe_mode()
    scheduler.set_relay(RELAYCO2_PIN, True)
    gpio.set_digital(LIGHT_PWMPIN, True)

    with patch("app.scheduler.time.monotonic", return_value=101.0):
        scheduler.update_fish_safe_mode()

    assert not relay_is_on(gpio, RELAYCO2_PIN)
    assert gpio._digital_values[LIGHT_PWMPIN] is False


def test_fish_safe_sos_uses_whole_second_dot_dash_pattern():
    scheduler, _ = make_scheduler()

    assert scheduler._fish_safe_sos_state(0) is True
    assert scheduler._fish_safe_sos_state(1) is False
    assert scheduler._fish_safe_sos_state(6) is True
    assert scheduler._fish_safe_sos_state(8) is True
    assert scheduler._fish_safe_sos_state(9) is False
    assert scheduler._fish_safe_sos_state(18) is True
    assert scheduler._fish_safe_sos_state(24) is False
    assert scheduler._fish_safe_sos_state(26) is False
    assert scheduler._fish_safe_sos_state(27) is True


def test_stopping_fish_safe_mode_turns_emergency_relays_off():
    scheduler, gpio = make_scheduler()
    with patch("app.scheduler.time.monotonic", return_value=100.0):
        scheduler.start_fish_safe_mode()
    scheduler.stop_fish_safe_mode()

    assert not relay_is_on(gpio, RELAYLIGHT_PIN)
    assert not relay_is_on(gpio, RELAYCO2_PIN)
    assert not relay_is_on(gpio, RELAYMOON_PIN)