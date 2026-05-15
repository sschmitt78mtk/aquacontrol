"""Tests for CRUD layer - schedule, settings, temperature history."""

import os
import sys
import tempfile
import pytest

# Ensure app module is importable
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.crud import CrudManager, ScheduleEntry, TemperatureHistory, DATA_DIR
from app.config import Settings, get_settings


@pytest.fixture
def crud():
    """Create a fresh CrudManager for each test."""
    mgr = CrudManager()
    mgr.load_all()
    yield mgr


def test_schedule_add_and_retrieve(crud):
    """Test adding schedule entries and retrieving them."""
    entries = [
        {"hour": 8, "minute": 0, "brightness": 2, "fadeDuration": 30, "device": 0},
        {"hour": 12, "minute": 0, "brightness": 5, "fadeDuration": 10, "device": 0},
        {"hour": 20, "minute": 0, "brightness": 1, "fadeDuration": 15, "device": 2},
    ]
    crud.update_schedule(entries)
    retrieved = crud.get_schedule()
    assert len(retrieved) == 3
    assert retrieved[0].hour == 8
    assert retrieved[0].brightness == 2
    assert retrieved[0].device == 0


def test_temperature_history(crud):
    """Test adding and retrieving temperature history."""
    import time
    # Use a fresh TemperatureHistory to avoid index offset from loaded pickle data
    from app.crud import TemperatureHistory
    crud.temperature = TemperatureHistory()
    ts = int(time.time())
    crud.add_temperature_entry(ts, 25.5)
    crud.add_temperature_entry(ts + 60, 26.0)

    hist = crud.get_temperature_history()
    # Entries should be at index 0 and 1 in the fresh circular buffer
    assert hist.timestamps[0] == ts
    assert hist.timestamps[1] == ts + 60


def test_temperature_csv(crud):
    """Test CSV generation."""
    import time
    ts = int(time.time())
    crud.add_temperature_entry(ts, 25.0)
    crud.add_temperature_entry(ts + 60, 26.5)

    csv = crud.get_temperature_csv()
    assert csv.startswith("\ufeff")
    assert "timestamp" in csv
    assert "25.0" in csv or "25,0" in csv


def test_clear_temperature_history(crud):
    """Test clearing temperature history."""
    crud.add_temperature_entry(100, 25.0)
    crud.clear_temperature_history()
    hist = crud.get_temperature_history()
    assert sum(hist.timestamps) == 0


def test_settings_update(crud):
    """Test updating settings."""
    # Get initial temp threshold
    original = get_settings().temp_alarmhigh_treshold
    crud.update_settings({"temp_alarmhigh_treshold": 30.0})
    assert get_settings().temp_alarmhigh_treshold == 30.0


def test_schedule_entry_dataclass():
    """Test ScheduleEntry dataclass."""
    e = ScheduleEntry(hour=14, minute=30, brightness=3, fadeMinutes=20, device=1)
    assert e.hour == 14
    assert e.minute == 30
    assert e.brightness == 3
    assert e.fadeMinutes == 20
    assert e.device == 1


def test_temperature_history_dataclass():
    """Test TemperatureHistory dataclass."""
    th = TemperatureHistory()
    assert len(th.timestamps) == 700
    assert len(th.history) == 700
    assert th.index == 0
