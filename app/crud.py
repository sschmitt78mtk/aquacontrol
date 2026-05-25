"""CRUD operations with pickle persistence - port of ESP8266 EEPROM storage."""

import pickle
import os
from typing import Optional
from dataclasses import dataclass, field

from app.config import Settings, get_settings
from app.temperature import temp_float2int, temp_int2float

# Default storage path relative to project root
DATA_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), "data")
os.makedirs(DATA_DIR, exist_ok=True)

SETTINGS_FILE = os.path.join(DATA_DIR, "settings.pickle")
SCHEDULE_FILE = os.path.join(DATA_DIR, "schedule.pickle")
TEMPERATURE_FILE = os.path.join(DATA_DIR, "temperature.pickle")

HISTORY_SIZE = 3000 # ~ 4 Weeks
LOG_ENTRY_SIZE = 5  # 4 bytes timestamp + 1 byte temp deviation


@dataclass
class ScheduleEntry:
    hour: int = 0
    minute: int = 0
    brightness: int = 0
    fadeMinutes: int = 0
    device: int = 0


@dataclass
class TemperatureHistory:
    timestamps: list = field(default_factory=lambda: [0] * HISTORY_SIZE)
    history: list = field(default_factory=lambda: [0] * HISTORY_SIZE)
    index: int = 0

    def to_csv(self) -> str:
        """Generate CSV string with UTF-8 BOM."""
        csv = "\ufefftimestamp;temp\n"
        for i in range(HISTORY_SIZE):
            idx = (self.index + i) % HISTORY_SIZE
            if self.timestamps[idx] != 0:
                from datetime import datetime
                ts = datetime.fromtimestamp(self.timestamps[idx])
                temp = temp_int2float(self.history[idx])
                csv += f"{ts.strftime('%Y-%m-%d %H:%M:%S')};{temp:.1f}\n".replace('.', ',')
        return csv


class CrudManager:
    """Manages persistence of settings, schedule, and temperature data."""

    def __init__(self):
        self.settings: Optional[Settings] = None
        self.schedule: list[ScheduleEntry] = []
        self.temperature: TemperatureHistory = TemperatureHistory()

    def load_all(self):
        """Load all data from pickle files."""
        self.settings = self._load_pickle(SETTINGS_FILE, Settings)
        self.schedule = self._load_pickle(SCHEDULE_FILE, [])
        self.temperature = self._load_pickle(TEMPERATURE_FILE, TemperatureHistory())

        # Sync loaded settings into the config module
        if isinstance(self.settings, Settings):
            s = get_settings()
            for key, value in self.settings.__dict__.items():
                if hasattr(s, key):
                    setattr(s, key, value)

    def save_all(self):
        """Save all data to pickle files."""
        self._save_pickle(SETTINGS_FILE, get_settings())
        self._save_pickle(SCHEDULE_FILE, self.schedule)
        self._save_pickle(TEMPERATURE_FILE, self.temperature)

    def get_settings(self) -> Settings:
        return get_settings()

    def update_settings(self, data: dict):
        from app.config import update_settings_from_dict
        update_settings_from_dict(data)
        self._save_pickle(SETTINGS_FILE, get_settings())

    def get_schedule(self) -> list[ScheduleEntry]:
        return self.schedule

    def update_schedule(self, entries: list[dict]):
        self.schedule = []
        for e in entries:
            self.schedule.append(ScheduleEntry(
                hour=e.get("hour", 0),
                minute=e.get("minute", 0),
                brightness=e.get("brightness", 0),
                fadeMinutes=e.get("fadeDuration", 0),
                device=e.get("device", 0),
            ))
        self._save_pickle(SCHEDULE_FILE, self.schedule)

    def get_temperature_history(self) -> TemperatureHistory:
        return self.temperature

    def add_temperature_entry(self, timestamp: int, temp: float):
        """Add a temperature reading to the circular buffer.
        Note: Does NOT auto-save to disk. The background loop handles
        periodic saves via save_all() at backupInterval_mins, matching
        the ESP8266 behavior where saveToEEPROM() is only called explicitly."""
        self.temperature.timestamps[self.temperature.index] = timestamp
        self.temperature.history[self.temperature.index] = temp_float2int(temp)
        self.temperature.index = (self.temperature.index + 1) % HISTORY_SIZE

    def clear_temperature_history(self):
        self.temperature = TemperatureHistory()
        self._save_pickle(TEMPERATURE_FILE, self.temperature)

    def get_temperature_csv(self) -> str:
        return self.temperature.to_csv()

    def _load_pickle(self, filepath: str, default):
        try:
            with open(filepath, "rb") as f:
                return pickle.load(f)
        except (FileNotFoundError, pickle.UnpicklingError, EOFError):
            return default

    def _save_pickle(self, filepath: str, data):
        try:
            with open(filepath, "wb") as f:
                pickle.dump(data, f)
        except Exception as e:
            print(f"[CRUD] Error saving {filepath}: {e}")


# Global singleton
_crud: CrudManager | None = None


def get_crud() -> CrudManager:
    global _crud
    if _crud is None:
        _crud = CrudManager()
    return _crud
