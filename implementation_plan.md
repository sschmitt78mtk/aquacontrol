# Implementation Plan - AquaControl Raspberry Pi Zero Port

## Summary
Port the ESP8266 aquarium controller to Raspberry Pi Zero using Python + FastAPI.
All core modules are implemented with 38 passing tests.

## Architecture

```
src/
├── app/
│   ├── __init__.py          # Package init
│   ├── config.py            # Settings + .env → config
│   ├── models.py            # Pydantic models for API
│   ├── time_utils.py        # Time utilities (ported from timestuff.h)
│   ├── temperature.py       # DS18B20 + simulation
│   ├── fader.py             # PWM soft-fader (ported from backlight_fader.cpp)
│   ├── gpio_interface.py    # GPIO abstraction (RPi / Mock / Abstract)
│   ├── crud.py              # CRUD → pickle persistence
│   ├── email_sender.py      # SMTP email with CSV attachments
│   ├── scheduler.py         # Schedule engine + GPIO control
│   └── main.py              # FastAPI app + background loop
├── emulator/
│   ├── __init__.py
│   └── gpio_emulator.py     # Re-exports MockGPIOController
├── static/
│   └── index.html           # Dashboard frontend (tabs: Status, Schedule, Params, Temp)
├── tests/
│   ├── __init__.py
│   ├── conftest.py
│   ├── test_config.py       # 5 tests
│   ├── test_crud.py         # 7 tests
│   ├── test_fader.py        # 6 tests
│   ├── test_gpio.py         # 6 tests
│   ├── test_models.py       # 4 tests
│   ├── test_temperature.py  # 4 tests
│   └── test_time_utils.py   # 6 tests
├── data/                    # (auto-created) pickle storage
├── requirements.txt
├── .env / .env.example
└── implementation_plan.md
```

## Key Design Decisions

1. **GPIO Abstraction**: `GPIOController` ABC with `RPiGPIOController` and `MockGPIOController`.
   - Mock auto-selected on non-RPi systems; RPi auto-selected via `gpiozero` / `RPi.GPIO`

2. **Persistence**: Pickle files in `data/` directory (replaces EEPROM).
   - Separate files: `settings.pickle`, `schedule.pickle`, `temperature.pickle`
   - HTTP `POST /api/parameters` + `POST /api/schedule` trigger persistence

3. **Background Loop**: Async `background_loop()` via FastAPI lifespan.
   - 1-second intervals for PWM updates
   - Temperature measurement every `Temp_Update_Interval_X_mins`
   - Schedule checks every minute
   - Auto-backup every `backupInterval_mins`
   - Weekly report + temperature alarm emails

4. **Fader**: Pure-Python port of the C++ backlight_fader using `time.monotonic()`.
   - 10-bit PWM (0-1023), linear interpolation with 64-bit arithmetic

5. **Active-LOW Relays**: Same logic as ESP8266 `setRelay(pin, state)`:
   - `state=True` → pin LOW (relay ON)
   - `state=False` → pin HIGH (relay OFF)

## REST API Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/api/status` | Current system status |
| GET | `/api/parameters` | All settings |
| POST | `/api/parameters` | Update settings |
| GET | `/api/schedule` | Schedule entries |
| POST | `/api/schedule` | Update schedule |
| POST | `/api/device` | Manual device control |
| POST | `/api/reset` | Clear temperature history |
| GET | `/api/temperature/csv` | Download temperature CSV |
| GET | `/api/temperature/current` | Current temperature |
| GET | `/api/light-levels` | Available light levels |

## Run

```bash
# Activate venv
source .venv/bin/activate

# Install deps
pip install -r requirements.txt

# Run server
uvicorn app.main:app --host 0.0.0.0 --port 8080

# Run tests
python -m pytest tests/ -v
```

## Test Status
**38 tests passed** ✅
