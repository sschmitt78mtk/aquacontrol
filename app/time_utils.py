"""Time utilities - port of ESP8266 timestuff.h."""

from datetime import datetime, timezone, timedelta


def get_day() -> int:
    """Get current day of month (1-31)."""
    return datetime.now().day


def get_month() -> int:
    """Get current month (1-12)."""
    return datetime.now().month


def get_day_of_week() -> int:
    """Get current day of week (0=Sunday, 6=Saturday)."""
    return datetime.now().weekday()  # Monday=0..Sunday=6; convert
    # Python: Monday=0..Sunday=6
    # ESP8266: Sunday=0..Saturday=6


def get_esp_day_of_week() -> int:
    """Get day of week in ESP8266 format (0=Sunday, 6=Saturday)."""
    return (datetime.now().weekday() + 1) % 7


def get_formatted_date() -> str:
    """Return date as 'dd.mm.yyyy'."""
    return datetime.now().strftime("%d.%m.%Y")


def get_correct_timestamp() -> int:
    """Return Unix timestamp (seconds since epoch)."""
    return int(datetime.now().timestamp())


def get_last_sunday_of_month(year: int, month: int) -> int:
    """Calculate the day of the last Sunday in a given month. (Zeller's congruence)"""
    import calendar
    last_day = calendar.monthrange(year, month)[1]
    # weekday of the last day (0=Monday, 6=Sunday)
    last_weekday = datetime(year, month, last_day).weekday()
    # days to subtract to get to Sunday (6 = Sunday in Python weekday)
    days_to_subtract = (last_weekday - 6) % 7
    return last_day - days_to_subtract


def adjust_for_dst() -> int:
    """Calculate DST offset for Germany.
    Returns 3600 (CET) or 7200 (CEST).
    Note: RPi system time handles DST natively via systemd-timesyncd.
    This function is provided for verification / manual override."""
    now = datetime.now()
    year = now.year
    month = now.month
    day = now.day
    day_of_week = now.weekday()  # 0=Monday

    last_sunday_march = get_last_sunday_of_month(year, 3)
    last_sunday_october = get_last_sunday_of_month(year, 10)

    # Germany DST rules (CEST: last Sunday March 01:00 UTC -> last Sunday October 01:00 UTC)
    is_dst = (month > 3 and month < 10) or \
             (month == 3 and (day > last_sunday_march or (day == last_sunday_march))) or \
             (month == 10 and (day < last_sunday_october or (day == last_sunday_october)))

    return 7200 if is_dst else 3600
