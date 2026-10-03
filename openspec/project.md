# Project Context

## Purpose
AquaControl is a Raspberry Pi Zero based aquarium controller: a **Python/FastAPI port of an
ESP8266 (Arduino/C++) aquarium controller**. It keeps fish and plants alive by:
- driving 5 devices: PWM-dimmable main light, PWM cooling fan, and three relays (light, CO2, moonlight)
- running time-based schedules with smooth linear PWM fading
- monitoring water temperature via a DS18B20 1-Wire sensor
- sending reboot notifications, temperature alarms, and weekly CSV reports by email
- failing safe ("fish-safe mode") when the clock is invalid at startup

This is a **port**: behavior, web routes, parameter names, and edge cases are intentionally kept
compatible with the original ESP8266 firmware.

## Tech Stack
- **Language/runtime:** Python 3.12 (developed in a `.venv` virtualenv)
- **Web framework:** FastAPI (`>=0.110`) served by uvicorn (`uvicorn[standard] >=0.27`), ASGI app `app.main:app`
- **Data models:** Pydantic v2 (`>=2.0`) for API request/response models (`app/models.py`)
- **Config/credentials:** python-dotenv (`>=1.0`) reading `.env`; runtime settings in a `Settings` dataclass
- **GPIO:** `gpiozero` (preferred) with an `RPi.GPIO` fallback, hidden behind `GPIOController`; `MockGPIOController` for non-RPi dev. (`gpiozero` is intentionally not pinned in `requirements.txt`; install on the Pi.)
- **Persistence:** stdlib `pickle`, three files in `data/` — replaces ESP8266 EEPROM
- **Email:** stdlib `smtplib` + `email.mime` over `SMTP_SSL`
- **Temperature:** Linux 1-Wire kernel driver via `/sys/bus/w1/devices/28-*/w1_slave`
- **Time:** system clock + `systemd-timesyncd`; `timedatectl` queried for NTP status; German DST logic in `app/time_utils.py`
- **Frontend:** plain static HTML/CSS/vanilla JS (no framework/bundler) in `static/`, served by FastAPI; UI text is German
- **Testing:** pytest (`tests/`)
- **Deployment:** systemd unit `aquacontrol.service`; uvicorn on `0.0.0.0:8080`
- **Target hardware:** Raspberry Pi Zero (single core, low RAM)

## Project Conventions

### Code style
- One module per concern in `app/`; module docstrings name the ESP8266 source file ported from.
- Type hints on signatures; PEP 604/585 unions/generics (`str | None`, `dict[int, int]`).
- Internal state as `@dataclass`; API contracts as Pydantic `BaseModel`.
- Global singletons via factory functions: `get_settings()`, `get_crud()`, `get_gpio()`.
- `logging.getLogger(__name__)`; messages prefixed with a bracketed tag (`[SCHEDULE]`, `[EMAIL]`, `[CRUD]`, `[FISH-SAFE]`, `[LOOP]`).
- Preserve ESP8266 identifiers and wire names even with typos (`temp_alarmhigh_treshold`) and JSON field names (`fadeDuration` inbound, `fadeMinutes` internal).
- PWM is 10-bit (0-1023). Relays are **active-LOW** (`scheduler.set_relay` inverts); PWM pins use standard logic.
- Comments/docstrings referencing the ESP8266 origin are expected and welcome.

### Architecture patterns
- Layered: `main.py` (FastAPI routes) -> `scheduler.py`/`fader.py` (behavior) -> `gpio_interface.py` (hardware); `config.py`, `crud.py`, `temperature.py`, `email_sender.py` are support services.
- Hardware abstraction + mock is the core testability pattern: logic never touches GPIO directly, only `get_gpio()`.
- A single async `background_loop()` in `main.py` ticks ~1 s (replaces the Arduino `loop()`): advances PWM faders, samples temperature, checks the schedule, runs safety checks, and does periodic backups.
- FastAPI lifespan runs `initialize_startup_mode()`, which selects normal vs. fish-safe startup.
- **Fish-safe mode:** if NTP is not synchronized at startup (or `SIMULATE_INVALID_TIME` is set), force CO2/PWM off, keep the main light on for 10 h of each 24 h *monotonic* cycle, blink SOS on moonlight, and auto-recover when NTP syncs (re-checked every 30 s) or when time is set manually via `/set-time`.

### Testing strategy
- pytest; one `test_*.py` per module. Each test file prepends the repo root to `sys.path` (plus `tests/conftest.py`) so `app` imports.
- Tests are hardware-free: use `MockGPIOController` and `unittest.mock.patch` (e.g. patch `app.scheduler.time.monotonic`, `app.main.subprocess.run`).
- Prefer small units; assert behavior, ranges, and edge cases (invalid timestamps, DST, PWM clamping, fish-safe timing).
- Run: `python -m pytest tests/ -v`; keep the whole suite green.

### Git workflow
- Long-lived branches: `main` (upstream ESP8266 reference — not the port) and `raspi-zero` (active RPi-port development). Do work on `raspi-zero`.
- Commit messages: short, imperative, lowercase, often Conventional-Commit style (`chore:`, `fix ...`, `added ...`, `remove ...`).
- Never commit `.env`, `*.pickle`, `.venv/`, `__pycache__/` (see `.gitignore`).

## Domain Context
- **Device codes (API):** `0` PWM light, `1` PWM cooling, `2` relay light, `3` relay CO2, `4` relay moonlight.
- **Light levels:** 7 discrete steps `[0, 50, 150, 250, 512, 860, 1023]` (0/5/15/25/50/84/100 %).
- **Schedule:** up to 40 entries `(hour, minute, brightness, fadeMinutes, device)`; fades are linear interpolations.
- **Temperature storage:** circular buffer `HISTORY_SIZE = 3000`; each entry = 4-byte Unix timestamp + 1-byte deviation from 25 degC (x10). CSV export uses a UTF-8 BOM, `;` delimiter, German decimal comma.
- **Pin map (BCM):** light PWM 12, cooling PWM 13, relay light 5, relay CO2 6, relay moon 16, water-level `COOLING_OFF_PIN` 26; DS18B20 on 1-Wire (default GPIO4, set in `/boot/config.txt`).
- **Safety:** cooling auto-shuts off after `maxcooling_mins` or when the water-level sensor reads low; everything turns off at 23:00.
- **Language:** UI and email content are German ("Neustart - Temperaturdaten", "Alarm Temperatur zu hoch oder zu niedrig", "Woechentlicher Temperaturreport").

## Important Constraints
- **This is a port:** preserve ESP8266 behavior, route names, parameter names, and edge cases unless a change is explicitly agreed.
- **Raspberry Pi Zero target** — single core, limited RAM; keep the ~1 s loop cheap and avoid heavy dependencies.
- **Never hard-fail on a bad clock:** out-of-range/invalid timestamps must be skipped (`crud._timestamp_to_datetime`) and startup must fall back to fish-safe mode.
- **Hardware stays mockable** — no direct GPIO/`subprocess`/SMTP calls from pure logic.
- **Privileged ops** (`sudo timedatectl set-time`, `sudo reboot`) run via validated, time-bounded `subprocess` calls.
- **Changing `HISTORY_SIZE` does not resize an existing `data/temperature.pickle`** (the pickle stores data, not the constant) — it only sizes a freshly created buffer.
- **Credentials precedence:** `.env` supplies defaults, but values saved from the Web UI win (`load_credentials()`).

## External Dependencies
- **SMTP server** (default Gmail `smtp.gmail.com:465`, SMTP_SSL) — credentials via `.env` / Web UI.
- **systemd / systemd-timesyncd** — NTP status via `timedatectl`; `sudo` for `set-time` and `reboot`; service unit `aquacontrol.service`.
- **1-Wire kernel driver** — DS18B20 via `/sys/bus/w1/devices`.
- **GPIO hardware** — `gpiozero` (preferred, install on Pi) or `RPi.GPIO`.
- **Optional:** `psutil` (for the `/ram` page). `gpiozero` and `psutil` are intentionally not pinned in `requirements.txt`.