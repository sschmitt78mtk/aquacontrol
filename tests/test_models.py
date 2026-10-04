"""Tests for Pydantic models."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.models import (
    ScheduleEntryIn, ScheduleEntryOut, DeviceStateRequest,
    StatusResponse, ParameterUpdateRequest
)


def test_schedule_entry_in():
    """Test ScheduleEntryIn model."""
    e = ScheduleEntryIn(hour=8, minute=30, brightness=3, fadeDuration=20, device=0)
    assert e.hour == 8
    assert e.minute == 30
    assert e.brightness == 3


def test_device_state_request():
    """Test DeviceStateRequest model."""
    r = DeviceStateRequest(device=2, value=1)
    assert r.device == 2
    assert r.value == 1


def test_status_response():
    """Test StatusResponse model."""
    s = StatusResponse(
        time="12:00:00", date="15.05.2026", temp=25.5,
        LIGHTPWM=512, COOLINGPWM=0,
        RELAYLIGHT=1, RELAYCO2=0, RELAYMOON=0
    )
    assert s.temp == 25.5
    assert s.LIGHTPWM == 512


def test_parameter_update_request():
    """Test ParameterUpdateRequest with partial update."""
    r = ParameterUpdateRequest(temp_alarmhigh_treshold=30.0)
    assert r.temp_alarmhigh_treshold == 30.0
    assert r.measure is None  # not provided


def test_parameter_update_request_simulate_sensor():
    """Test that simulateSensor is accepted and survives a partial update.

    Regression test: simulateSensor was missing from the model, so the value
    sent by the settings web UI was silently dropped and the checkbox always
    reverted to its default (True).
    """
    r = ParameterUpdateRequest(simulateSensor=False)
    assert r.simulateSensor is False

    dumped = {k: v for k, v in r.model_dump().items() if v is not None}
    assert dumped == {"simulateSensor": False}
