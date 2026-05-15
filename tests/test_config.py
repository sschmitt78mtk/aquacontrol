"""Tests for configuration."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.config import Settings, get_settings, settings_to_dict, update_settings_from_dict, load_credentials


def test_settings_defaults():
    """Test default settings values."""
    s = Settings()
    assert s.temp_alarmhigh_treshold == 28.5
    assert s.temp_alarmlow_treshold == 19.5
    assert s.simulateSensor == True
    assert s.maxcooling_mins == 180
    assert s.pwmfrequency == 1000


def test_settings_singleton():
    """Test that get_settings returns same instance."""
    s1 = get_settings()
    s2 = get_settings()
    assert s1 is s2


def test_settings_to_dict():
    """Test serialization to dict."""
    d = settings_to_dict()
    assert isinstance(d, dict)
    assert "temp_alarmhigh_treshold" in d
    assert "simulateSensor" in d


def test_update_settings_from_dict():
    """Test updating settings from dict."""
    update_settings_from_dict({"temp_alarmhigh_treshold": 30.0, "simulateSensor": False})
    s = get_settings()
    assert s.temp_alarmhigh_treshold == 30.0
    assert s.simulateSensor == False
    # Reset
    update_settings_from_dict({"temp_alarmhigh_treshold": 28.5, "simulateSensor": True})


def test_load_credentials():
    """Test loading credentials from environment."""
    creds = load_credentials()
    assert "smtp_host" in creds
    assert "smtp_port" in creds
    assert "recipient_email" in creds
