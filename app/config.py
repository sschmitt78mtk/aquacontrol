"""Configuration module - ported from ESP8266 'parameter' struct + .env credentials."""

import os
from dataclasses import dataclass, asdict
from dotenv import load_dotenv

load_dotenv()


@dataclass
class Settings:
    """System parameters - mirrors the ESP8266 'parameter' struct."""
    # Temperature thresholds
    temp_alarmhigh_treshold: float = 28.5
    temp_alarmlow_treshold: float = 19.5

    # Update intervals (minutes)
    Temp_Update_Interval_LIVE_mins: int = 20
    Temp_Update_Interval_SIM_mins: int = 1
    backupInterval_mins: int = 240
    maxcooling_mins: int = 180

    # Operating modes
    simulateSensor: bool = True
    emailme: bool = True
    skipmail: bool = True
    serialout: bool = True
    measure: bool = True

    # Technical settings
    pwmfrequency: int = 1000
    weeklyReport_tm_wday: int = 5
    weeklyReport_tm_hour: int = 22
    weeklyReport_tm_min: int = 0

    # Network settings (loaded from .env but also persisted in pickle)
    ssid: str = ""
    password: str = ""
    ntpServer: str = "fritz.box"

    # Email settings (loaded from .env but also persisted in pickle)
    smtp_AUTH_EMAIL: str = ""
    smtp_AUTH_PASSWORD: str = ""
    smtp_RECIPIENT_EMAIL: str = ""



# Global singleton instance
_settings: Settings | None = None


def get_settings() -> Settings:
    global _settings
    if _settings is None:
        _settings = Settings()
    return _settings


def load_credentials() -> dict[str, str | int]:
    """Load SMTP and other credentials from .env file."""
    return {
        "smtp_host": os.getenv("SMTP_HOST", "smtp.gmail.com"),
        "smtp_port": int(os.getenv("SMTP_PORT", "465")),
        "smtp_auth_email": os.getenv("SMTP_AUTH_EMAIL", ""),
        "smtp_auth_password": os.getenv("SMTP_AUTH_PASSWORD", ""),
        "recipient_email": os.getenv("RECIPIENT_EMAIL", ""),
        "ntp_server": os.getenv("NTP_SERVER", "fritz.box"),
        "secret_key": os.getenv("SECRET_KEY", ""),
    }


def settings_to_dict() -> dict:
    """Return settings as a plain dict (for JSON serialization)."""
    return asdict(get_settings())


def update_settings_from_dict(data: dict) -> Settings:
    """Update settings from a dict (e.g., from API request)."""
    s = get_settings()
    for key, value in data.items():
        if hasattr(s, key):
            setattr(s, key, value)
    return s
