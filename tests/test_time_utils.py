"""Tests for time utility functions."""

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(__file__))))

from app.time_utils import (
    get_day, get_month, get_day_of_week, get_esp_day_of_week,
    get_formatted_date, get_last_sunday_of_month, adjust_for_dst
)
from datetime import datetime


def test_get_day():
    """Test day of month."""
    d = get_day()
    assert 1 <= d <= 31


def test_get_month():
    """Test month."""
    m = get_month()
    assert 1 <= m <= 12


def test_day_of_week():
    """Test day of week conversion."""
    dow = get_day_of_week()
    assert 0 <= dow <= 6
    esp_dow = get_esp_day_of_week()
    assert 0 <= esp_dow <= 6
    # ESP Sunday (0) = Python Monday (0) -> esp = (0+1)%7 = 1
    # Actually the mapping is:
    # Python Monday=0..Sunday=6
    # ESP Sunday=0..Saturday=6
    # So conversion: esp = (python_dow + 1) % 7
    assert esp_dow == (dow + 1) % 7


def test_formatted_date():
    """Test date format."""
    d = get_formatted_date()
    assert len(d) == 10
    assert d[2] == '.'
    assert d[5] == '.'


def test_last_sunday_of_month():
    """Test last Sunday calculation."""
    # March 2026: last day is Tue (31st), last Sunday should be the 29th
    ls = get_last_sunday_of_month(2026, 3)
    # March 2026 - check: March 1 2026 is Sunday
    # So Sundays: 1, 8, 15, 22, 29
    assert ls == 29, f"Expected 29, got {ls}"

    # October 2026: last day is Sat (31st), last Sunday = 25
    ls = get_last_sunday_of_month(2026, 10)
    # Oct 2026 - check: 1 Oct is Thu
    # Sundays: 4, 11, 18, 25
    assert ls == 25, f"Expected 25, got {ls}"


def test_adjust_for_dst():
    """Test DST calculation returns reasonable value."""
    dst = adjust_for_dst()
    # Should be either 3600 (CET) or 7200 (CEST)
    assert dst in (3600, 7200)
