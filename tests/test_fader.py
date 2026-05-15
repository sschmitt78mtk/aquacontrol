"""Tests for PWM Fader."""

import os
import sys
import time
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.fader import PWMFader


def test_fader_initial_state():
    """Test fader starts at 0 and not busy."""
    f = PWMFader()
    assert f.brightness == 0
    assert f.busy == False


def test_fader_immediate_completion():
    """Test fader with 0 duration reaches target immediately."""
    f = PWMFader()
    f.fade(512, 0)
    # After fade with 0 duration, should be done after one upd() call
    assert f.busy == True
    changed = f.upd()
    assert changed == True
    assert f.brightness == 512
    assert f.busy == False


def test_fader_target_same_as_current():
    """Test fade to same brightness doesn't start."""
    f = PWMFader()
    assert f.brightness == 0
    f.fade(0, 2000)
    assert f.busy == False


def test_fader_eventually_reaches_target():
    """Test fader reaches target within reasonable time."""
    f = PWMFader()
    f.fade(1023, 10)  # 10ms
    max_iter = 100
    while f.busy and max_iter > 0:
        f.upd()
        time.sleep(0.001)
        max_iter -= 1
    assert f.brightness == 1023, f"Expected 1023, got {f.brightness}"
    assert f.busy == False


def test_fader_brightness_range():
    """Test brightness stays within 0-1023."""
    f = PWMFader()
    # Try impossible large fade
    f._brightness_started = 0
    result = f._calc_brightness(time.monotonic(), 100, 500, 1000)
    assert 0 <= result <= 1023


def test_fader_multiple_fades():
    """Test calling fade multiple times."""
    f = PWMFader()
    f.fade(512, 1000)
    f.fade(256, 500)  # Override before previous completes
    assert f._brightness_target == 256
