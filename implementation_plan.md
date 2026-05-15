# AquaControl Raspberry Pi Zero Port - Implementation Plan

## Overview
Port the ESP8266 Arduino code (ESP8266-Code) to a Python/FastAPI application running on Raspberry Pi Zero. Both CONTROL and MONITOR features run simultaneously (no RAM limitations on RPi).

## Principles
- Minimal dependencies (FastAPI + uvicorn + pydantic + python-dotenv + smtplib (stdlib))
- Pickle for data persistence (replaces EEPROM)
- CRUD pattern for all API endpoints
- `.env` for sensitive credentials
- Keep existing folder structure (`app/`, `static/`, `tests/`, `emulator/`)
- GPIO abstraction layer for emulation/testing

---
## Refinements vs. Original ESP8266 Design

### 1. NTP / System Time
- **ESP8266**: Manual NTP client via WiFiUDP + NTPClient library + DST adjust
- **RPi Zero**: System time managed by `systemd-timesyncd`. The app uses Python's `datetime.now()` from system time. The `time_utils.py` module provides DST-aware formatting and the `adjust_for_dst()` logic is preserved as a system time verification utility, but the RPi kernel handles actual UTC<->local time conversion.

### 2. GPIO Library Choice
- **ESP8266**: `analogWrite()` + `digitalWrite()` (Arduino HAL)
- **RPi Zero**: Use `gpiozero` as primary library (works with all Pi kernels, unlike `RPi.GPIO` which has kernel compatibility issues). Fallback to `MockGPIOController` when not on Pi hardware.

### 3. Frontend API Path Alignment
- **ESP8266/Arduino endpoints** (used in HTML JS): `/status`, `/getSchedule`, `/setSchedule`, `/getParameters`, `/saveParams`, `/setDeviceState`, `/csv`
- **New RESTful endpoints** (from plan): `/api/status`, `/api/schedule` (GET), `/api/schedule` (POST), `/api/parameters` (GET/POST), `/api/device` (POST), `/api/temperature/csv`
- **Action**: The frontend HTML files will be updated to call the new RESTful paths.

### 4. Relay Logic (Active-LOW)
- Verified correct: `digitalWrite(pin, state ? LOW : HIGH)` for active-LOW relays
- `RELAYMOON_PIN` = `LED_BUILTIN` (D4) on NodeMCU

### 5. PWM Output on RPi
- **ESP8266**: `analogWrite()` with 10-bit resolution
- **RPi Zero**: `gpiozero.PWMOutputDevice` or software PWM via `pigpio`. RPi has no native hardware PWM on all GPIOs, but software PWM is adequate for light/cooling control (< 1kHz).

---

## Step 1: Project Scaffolding

### 1.1 Create `requirements.txt`
```
fastapi>=0.110.0
uvicorn[standard]>=0.27.0
python-dotenv>=1.0.0
pydantic>=2.0.0
gpiozero>=2.0.0    # Optional - only on RPi
```

### 1.2 Create `.env` file (gitignored template)
```
# WiFi Credentials (for reference only - RPi uses system networking)
SSID=fritzzzz
PASSWORD=51...0

# SMTP Settings
SMTP_HOST=smtp.gmail.com
SMTP_PORT=465
SMTP_AUTH_EMAIL=sc..2@gmail.com
SMTP_AUTH_PASSWORD=your-app-password
RECIPIENT_EMAIL=s...8@gmx.de

# NTP
NTP_SERVER=fritz.box

# Application
SECRET_KEY=change-this-to-random-string
```

### 1.3 Create `.env.example` (committed to git)
Same as `.env` but with placeholder values.

### 1.4 Update `.gitignore`
Add `.env`, `*.db`, `__pycache__/`, `*.pickle`

### 1.5 Create directory structure
```
app/
  __init__.py
  main.py            # FastAPI app + startup + background tasks
  config.py          # Load .env, settings dataclass
  crud.py            # CRUD operations (pickle persistence)
  models.py          # Pydantic models for API
  fader.py           # PWM fade logic (port of backlight_fader.cpp)
  scheduler.py       # Schedule engine
  temperature.py     # DS18B20 sensor + simulation
  email_sender.py    # SMTP via smtplib
  time_utils.py      # DST, formatting (port of timestuff.h)
  gpio_interface.py  # GPIO abstraction (mockable)
static/
  index.html         # Direct control panel (port of html4light.h)
  schedule.html      # Schedule editor (port of htmltemplate.h)
  temperature.html   # Temperature graph (port of temperaturesvgpage.h)
  settings.html      # Settings page (port of htmlsettings.h)
  style.css          # Shared styles
emulator/
  __init__.py
  gpio_emulator.py   # Mock GPIO for development/testing
tests/
  __init__.py
  test_fader.py
  test_scheduler.py
  test_temperature.py
  test_crud.py
```

---

## Step 2: Configuration Module (`app/config.py`)

### 2.1 Create `Settings` dataclass
Port the ESP8266 `parameter` struct to a Python dataclass:
- `temp_alarmhigh_treshold: float = 28.5`
- `temp_alarmlow_treshold: float = 19.5`
- `Temp_Update_Interval_LIVE_mins: int = 20`
- `Temp_Update_Interval_SIM_mins: int = 1`
- `backupInterval_mins: int = 240`
- `maxcooling_mins: int = 180`
- `simulateSensor: bool = True`
- `emailme: bool = True`
- `skipmail: bool = True`
- `serialout: bool = True`
- `measure: bool = True`
- `pwmfrequency: int = 1000`
- `weeklyReport_tm_wday: int = 5`
- `weeklyReport_tm_hour: int = 22`
- `weeklyReport_tm_min: int = 0`

### 2.2 Load credentials from `.env`
Read via `python-dotenv`: smtp_host, smtp_port, smtp_auth_email, smtp_auth_password, recipient_email, ntpServer.

### 2.3 Provide `get_settings()` singleton
Returns the global settings instance.

---

## Step 3: CRUD Layer (`app/crud.py`)

### 3.1 Create `CrudManager` class
Uses pickle for persistence. Manages three storage files:
- `settings.pickle` - System parameters
- `schedule.pickle` - Schedule entries
- `temperature.pickle` - Temperature history

### 3.2 Methods
- `load_all()` - Load everything from pickle files on startup
- `save_all()` - Save everything to pickle files
- `get_settings()` / `update_settings(data)`
- `get_schedule()` / `update_schedule(entries)`
- `get_temperature_history()` / `add_temperature_entry(timestamp, temp)`
- `clear_temperature_history()`
- `get_temperature_csv()` - Generate CSV string

### 3.3 Data structures (port from ESP8266)
- `ScheduleEntry`: hour, minute, brightness, fadeMinutes, device
- Temperature history: circular buffer of (timestamp, temp) pairs
  - HISTORY_SIZE = 700
  - Store as deviation from 25°C in tenths of degrees (int8) to save space

### 3.4 `get_crud()` singleton function

---

## Step 4: Models (`app/models.py`)

### 4.1 Pydantic models for request/response
- `ScheduleEntryIn`: hour, minute, brightness, fadeDuration, device
- `ScheduleEntryOut`: hour, minute, brightness, fadeDuration, device (same fields)
- `DeviceStateRequest`: device (int), value (int)
- `StatusResponse`: time, date, temp, LIGHTPWM, COOLINGPWM, RELAYLIGHT, RELAYCO2, RELAYMOON
- `ParameterUpdateRequest`: all settings as optional fields

---

## Step 5: GPIO Interface (`app/gpio_interface.py`)

### 5.1 Create `GPIOController` abstract class
Defines interface:
- `set_pwm(pin, value)` - Set PWM duty cycle (0-1023)
- `set_digital(pin, state)` - Set digital output HIGH/LOW
- `get_digital(pin)` - Read digital input
- `cleanup()` - Reset all pins

### 5.2 Implement `RPiGPIOController` (real hardware)
Uses `gpiozero` (preferred) with fallback to `RPi.GPIO`. Falls back gracefully if import fails.

### 5.3 Implement `MockGPIOController` (emulator)
Stores pin states in memory. Used when running on non-RPi systems.

### 5.4 `get_gpio()` factory function
Returns MockGPIOController if GPIO libs not available, RPiGPIOController otherwise.

---

## Step 6: Fader Module (`app/fader.py`)

### 6.1 Port `backlight_fader.cpp` to Python class `PWMFader`
- `init()` - Reset state
- `fade(target_brightness: int, duration_ms: int)` - Start fade
- `upd() -> bool` - Calculate current brightness based on elapsed time. Returns True if value changed.
- Uses `time.monotonic()` instead of `millis()`
- 64-bit calculations to prevent overflow
- 10-bit PWM resolution (0-1023)

### 6.2 Test:
- Fade from 0 to 1023 over 1000ms
- Fade from 1023 to 0 over 500ms
- Interrupted fade (new target during active fade)
- Edge cases: already at target, zero duration

---

## Step 7: Temperature Module (`app/temperature.py`)

### 7.1 Create `TemperatureController`
- `read() -> float` - Read current temperature
- Two modes: real sensor (DS18B20) or simulation
- Real: read from `/sys/bus/w1/devices/28-*/w1_slave`
- Simulated: `random.uniform(19, 35)`

### 7.2 Temperature conversion helpers (port from ESP8266)
- `temp_float2int(absolutetemp) -> int` - Convert to deviation from 25°C * 10
- `temp_int2float(tempDeviation) -> float` - Convert back

---

## Step 8: Time Utilities (`app/time_utils.py`)

### 8.1 Port `timestuff.h` to Python
- `get_day()`, `get_month()`, `get_day_of_week()` - System time based
- `get_formatted_date()` - "dd.mm.yyyy"
- `get_correct_timestamp() -> int` - Unix timestamp
- `adjust_for_dst()` - Calculate if DST is active, return offset (3600 or 7200) — useful for verifying or manual override; RPi system time handles DST natively
- `get_last_sunday_of_month(year, month)` - Helper for DST calc

---

## Step 9: Email Sender (`app/email_sender.py`)

### 9.1 Create `EmailSender` class
- `send_email(is_reboot=False, is_alarm=False) -> bool`
- Build CSV attachment in memory (no LittleFS needed)
- Use `smtplib.SMTP_SSL` for port 465
- Use `email.mime` modules for multipart message with CSV attachment
- Simplified reconnection logic (RPi has stable networking)
- Weekly email scheduling
- Temperature alarm emails (once per day maximum, tracked by `AlarmSentDay`)

---

## Step 10: Schedule Engine (`app/scheduler.py`)

### 10.1 Port schedule checking logic
- `check_schedule(hour, minute)` - Iterate entries, execute matching ones
- Device actions based on `device` field (0=PWMLIGHT, 1=COOLING, 2=LIGHT, 3=CO2, 4=MOON)
- Uses `PWMFader` for fading devices (0, 1)
- Uses GPIO digital for relay devices (2, 3, 4)
- `set_outputs_according_to_schedule(hour, minute)` - Restore state on startup by replaying all entries up to current time
- `check_max_cooling_time()` - Auto-shutoff cooling after maxcooling_mins
- `off_all()` - Turn everything off (scheduled at 23:00)
- `lightLevels` array: [0, 50, 150, 250, 512, 860, 1023]
- Relay is set LOW (active) for brightness > 0, HIGH (inactive) for brightness == 0

---

## Step 11: FastAPI Application (`app/main.py`)

### 11.1 Create FastAPI app instance
- Title: "AquaControl V4"
- Configure CORS (if needed)
- Mount static files from `static/` directory

### 11.2 Startup event
- Initialize CRUD (load pickle data)
- Initialize GPIO controller
- Initialize PWMFaders for Light and Cooling
- Initialize TemperatureController
- Set outputs according to schedule (current time)
- Send startup email (if enabled)
- Create background tasks:
  - `temperature_polling_task` - Every N minutes (configured interval)
  - `schedule_check_task` - Every 60 seconds, checks schedule + cooling timeout + auto-off at 23:00 + weekly email
  - `backup_task` - Every backupInterval_mins, save data to pickle

### 11.3 API Endpoints

| Method | Path | Handler | Description | Compatible with ESP8266 JS |
|---|---|---|---|---|
| GET | `/` | Redirect to `/light` | Root -> control panel | - |
| GET | `/light` | Serve static file | Direct control panel | yes |
| GET | `/schedule` | Serve static file | Schedule editor | yes |
| GET | `/settings` | Serve static file | Settings page | yes |
| GET | `/info` | Serve static file | Temperature graph | yes |
| **GET** | **`/api/status`** | `get_status` | JSON status | replaces `/status` |
| **GET** | **`/api/schedule`** | `read_schedule` | Get schedule entries | replaces `/getSchedule` |
| **POST** | **`/api/schedule`** | `create_schedule` | Replace schedule entries | replaces `/setSchedule` |
| **GET** | **`/api/parameters`** | `read_parameters` | Get all parameters | replaces `/getParameters` |
| **POST** | **`/api/parameters`** | `update_parameters` | Update parameters | replaces `/saveParams` |
| **GET** | **`/api/temperature`** | `read_temperature` | Get temperature history (JSON) | - |
| **DELETE** | **`/api/temperature`** | `delete_temperature` | Clear temperature history | replaces `/deletehistory321` |
| **GET** | **`/api/temperature/csv`** | `read_temperature_csv` | Download CSV | replaces `/csv` |
| **POST** | **`/api/device`** | `set_device_state` | Control device | replaces `/setDeviceState` |
| **POST** | **`/api/email`** | `trigger_email` | Send test email | replaces `/email` |
| **GET** | **`/api/system`** | `get_system_info` | RAM/uptime info | replaces `/ram` |
| **POST** | **`/api/restart`** | `restart_system` | Reboot RPi | replaces `/restart` |

### 11.4 Background task implementations

#### Temperature polling task
```python
async def temperature_polling():
    while True:
        interval = settings.Temp_Update_Interval_mins * 60
        await asyncio.sleep(interval)
        temp = temperature_controller.read()
        timestamp = get_correct_timestamp()
        crud.add_temperature_entry(timestamp, temp)
        # Check alarm thresholds
        if temp > settings.temp_alarmhigh_treshold or temp < settings.temp_alarmlow_treshold:
            await email_sender.send_alarm(temp)
```

#### Schedule check task
```python
async def schedule_check():
    while True:
        await asyncio.sleep(60)  # Every minute
        now = datetime.now()
        scheduler.check_schedule(now.hour, now.minute)
        scheduler.check_max_cooling_time()
        # Auto-off at 23:00
        if now.hour == 23 and now.minute == 0:
            scheduler.off_all()
        # Weekly email
        if (now.weekday() == settings.weeklyReport_tm_wday and 
            now.hour == settings.weeklyReport_tm_hour and 
            now.minute == settings.weeklyReport_tm_min):
            await email_sender.send_weekly_report()
```

---

## Step 12: Frontend Pages (`static/`)

**Key change from ESP8266**: All frontend JS API calls updated to use new RESTful paths:
- `/status` → `/api/status`
- `/getSchedule` → `GET /api/schedule`
- `/setSchedule` → `POST /api/schedule`
- `/getParameters` → `GET /api/parameters`
- `/saveParams` → `POST /api/parameters`
- `/setDeviceState` → `POST /api/device`
- `/csv` → `/api/temperature/csv`
- `/email` → `/api/email`

### 12.1 `static/index.html` - Direct Control Panel
Port of `html4light.h`:
- Status display with live-updating device states
- PWM sliders for Light (device 0) and Cooling (device 1)
- Toggle buttons for relays (Light, CO2, Moonlight) - initially locked
- Calls `/api/status` for status updates
- Calls `POST /api/device` to set device states
- Tab navigation to other pages

### 12.2 `static/schedule.html` - Schedule Editor
Port of `htmltemplate.h`:
- Table with time pickers, brightness, fade duration, device selection
- Add/remove rows (max 20)
- SVG graph showing brightness profile over 24h
- Save button calls `POST /api/schedule`
- Load button calls `GET /api/schedule`
- Tab navigation

### 12.3 `static/temperature.html` - Temperature Graph
Port of `temperaturesvgpage.h`:
- Fetches CSV from `GET /api/temperature/csv`
- Client-side SVG rendering of temperature curve
- Day markers on X-axis
- Hover tooltip with time and temperature
- Right-click to download SVG
- Tab navigation

### 12.4 `static/settings.html` - Settings Page
Port of `htmlsettings.h`:
- Form with all configurable parameters
- Password visibility toggles
- Loads current values from `GET /api/parameters`
- Saves via `POST /api/parameters`
- Tab navigation

---

## Step 13: Emulator (`emulator/gpio_emulator.py`)

### 13.1 Software-based GPIO simulation
- Stores pin modes and values in dictionaries
- PWM simulation with software timing
- Console output showing pin state changes
- Used when running on non-RPi for testing

---

## Step 14: Tests (`tests/`)

### 14.1 `tests/test_fader.py`
- Test fade timing accuracy
- Test edge cases (same target, zero duration, interrupted fades)
- Test busy flag behavior

### 14.2 `tests/test_scheduler.py`
- Test schedule matching at specific times
- Test device control based on schedule entries
- Test max cooling timeout
- Test auto-off at 23:00

### 14.3 `tests/test_temperature.py`
- Test conversion functions (float→int, int→float)
- Test simulated temperature generation
- Test DS18B20 parsing (with mock file)

### 14.4 `tests/test_crud.py`
- Test save/load from pickle files
- Test circular buffer behavior for temperature history
- Test CSV generation

---

## Step 15: `app/__init__.py`

### 15.1 Package initialization
Export key symbols for easy importing.

---

## Execution Order Summary

| Step | Depends On | Estimated Effort |
|---|---|---|
| 1. Scaffolding | - | Small |
| 2. Config | 1 | Small |
| 3. CRUD | 1 | Medium |
| 4. Models | 1 | Small |
| 5. GPIO Interface | 1 | Medium |
| 6. Fader | 5 | Small |
| 7. Temperature | 5 | Small |
| 8. Time Utils | - | Small |
| 9. Email Sender | 2, 3, 8 | Medium |
| 10. Scheduler | 5, 6, 7, 8 | Medium |
| 11. Main (FastAPI + routes) | 2-10 | Large |
| 12. Frontend | - | Medium |
| 13. Emulator | 5 | Small |
| 14. Tests | 6, 7, 10, 3 | Medium |
| 15. Package init | all | Tiny |

Each step is designed to be independently testable. Steps within the same numbered section can be parallelized.
