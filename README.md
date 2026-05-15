# AquaControl – Raspberry Pi Zero Aquarium Controller

A Python/FastAPI port of the [ESP8266 aquarium controller](https://github.com/sschmitt78mtk/aquacontrol/tree/ESP8266), running on a Raspberry Pi Zero. Controls lighting (PWM + relay), cooling fans, CO2, and moonlight on a schedule, monitors water temperature via DS18B20, and sends email alerts and weekly reports with CSV attachments.

## Features

- **5 devices**: PWM light, PWM cooling, relay light, relay CO2, relay moonlight
- **7-step light levels**: 0%, 5%, 15%, 25%, 50%, 84%, 100% (10-bit PWM, 0–1023)
- **Smooth PWM fading**: Linear interpolation fader ported from C++ `backlight_fader`
- **Schedule engine**: Up to 40 timed entries with fade durations
- **Temperature monitoring**: DS18B20 via 1-Wire or simulated sensor
- **Email notifications**: Reboot alerts, temperature alarms, weekly CSV reports (SMTP_SSL)
- **Water level safety**: Auto-shutoff cooling when water level is low
- **DST handling**: German summer/winter time calculation
- **Web UI**: Static HTML pages at the same routes as the original ESP8266
- **REST API**: JSON endpoints for settings, schedule, device control, and temperature data
- **Auto-off at 23:00**: Scheduled safety shutdown

## Architecture

```
app/
├── __init__.py            # Package init
├── config.py              # Settings dataclass + .env credential loader
├── models.py              # Pydantic request/response models
├── time_utils.py          # Time utilities (ported from timestuff.h)
├── temperature.py         # DS18B20 reader + simulation + temp conversion
├── fader.py               # PWM soft-fader (ported from backlight_fader.cpp)
├── gpio_interface.py      # GPIO abstraction (RPi gpiozero / RPi.GPIO / Mock)
├── crud.py                # Persistence layer (pickle files, replaces EEPROM)
├── email_sender.py        # SMTP email with CSV attachments
├── scheduler.py           # Schedule engine + device control + safety checks
└── main.py                # FastAPI app + async background loop
emulator/
├── __init__.py
└── gpio_emulator.py       # Re-exports MockGPIOController
static/
├── index.html             # Navigation hub
├── light.html             # Device control (port of html4light.h)
├── schedule.html          # Schedule editor + SVG graph (port of htmltemplate.h)
├── settings.html          # Parameter settings form (port of htmlsettings.h)
└── temperature.html       # Temperature SVG chart (port of temperaturesvgpage.h)
tests/
├── conftest.py
├── test_config.py         # 5 tests
├── test_crud.py           # 7 tests
├── test_fader.py          # 6 tests
├── test_gpio.py           # 6 tests
├── test_models.py         # 4 tests
├── test_temperature.py    # 4 tests
└── test_time_utils.py     # 6 tests
data/                      # (auto-created) pickle storage
```

## ESP8266 → RPi Porting Map

| ESP8266 Module | ESP8266 File | Python Module | Key Changes |
|---|---|---|---|
| Web server | `ESP8266WebServer(80)` | `FastAPI + uvicorn` | Async, REST JSON API |
| EEPROM storage | `EEPROM.put/get` | `crud.py` (pickle) | 3 separate pickle files |
| PWM fader | `backlight_fader.cpp/hpp` | `fader.py` | `time.monotonic()` instead of `millis()` |
| Time/NTP/DST | `NTPClient + timestuff.h` | `time_utils.py` | RPi uses `systemd-timesyncd` |
| Temperature | `OneWire + DallasTemperature` | `temperature.py` | RPi 1-Wire sysfs interface |
| Email | `ESP_Mail_Client + LittleFS` | `email_sender.py` | `smtplib + email.mime` |
| GPIO | `digitalWrite/analogWrite` | `gpio_interface.py` | `gpiozero` / `RPi.GPIO` |
| HTML templates | `html*.h` (PROGMEM) | `static/*.html` | Separate static files |
| Schedule logic | `checkSchedule()` inline | `scheduler.py` | Same logic, class-based |

## Pin Mapping

| ESP8266 Pin | ESP8266 Function | RPi BCM Pin | RPi Function |
|---|---|---|---|
| D5 | PWM Light | GPIO5 | PWM Light |
| D6 | PWM Cooling | GPIO6 | PWM Cooling |
| D1 | Relay Light | GPIO1* | Relay Light (active-LOW) |
| D2 | Relay CO2 | GPIO2* | Relay CO2 (active-LOW) |
| LED_BUILTIN (D4) | Relay Moon | GPIO4 | Relay Moon (active-LOW) |
| D0 | Water level sensor | GPIO0* | Water level (HIGH=ok, LOW=low) |
| D7 | DS18B20 (1-Wire) | GPIO7 | DS18B20 (1-Wire sysfs) |

> ⚠️ **GPIO 0, 1, 2** are special-purpose on RPi (I2C, HAT EEPROM). Verify these pin assignments against your actual hardware wiring before deployment.

## Device Control Logic

- **Relay pins** (Light, CO2, Moon): Active-LOW — relay ON = pin LOW, relay OFF = pin HIGH
- **PWM pins** (Light, Cooling): Standard logic — LOW = off, HIGH = fully on
- **Water level sensor** (COOLING_OFF_PIN): HIGH = water OK, LOW = water too low → cooling auto-shutoff

## REST API Endpoints

### JSON API

| Method | Path | Description |
|---|---|---|
| GET | `/api/status` | Current system status (time, temp, PWM, relays) |
| GET | `/api/parameters` | All settings as JSON |
| POST | `/api/parameters` | Update settings (partial) |
| GET | `/api/schedule` | Schedule entries as JSON |
| POST | `/api/schedule` | Replace schedule entries |
| POST | `/api/device` | Manual device control (`{device: 0-4, value: 0-1023}`) |
| POST | `/api/reset` | Clear temperature history |
| GET | `/api/temperature/csv` | Download temperature CSV (UTF-8 BOM) |
| GET | `/api/temperature/current` | Current temperature reading |
| GET | `/api/light-levels` | Available light level PWM values |

### HTML / Action Routes

| Method | Path | Description |
|---|---|---|
| GET | `/` | Navigation hub |
| GET | `/light` | Device control page |
| GET | `/schedule` | Schedule editor page |
| GET | `/settings` | Parameter settings page |
| GET | `/info` | Temperature chart page |
| GET | `/email` | Send email (reboot notification) |
| GET | `/ram` | System RAM info |
| GET | `/save` | Manually save data to disk |
| GET | `/restart` | Save data and reboot system |

## Background Loop

The async `background_loop()` runs every ~1 second, matching the ESP8266 `loop()` pattern:

| Frequency | Action |
|---|---|
| Every second | Update PWM fader values → apply to GPIO |
| Every `Temp_Update_Interval` minutes | Read temperature, check alarms, store in circular buffer |
| Every minute | Check schedule, check max cooling time, check weekly report, check 23:00 auto-off |
| Every `backupInterval_mins` minutes | Save all data to pickle files |

## Settings

All settings mirror the original ESP8266 `parameter` struct:

| Setting | Default | Description |
|---|---|---|
| `temp_alarmhigh_treshold` | 28.5 | High temperature alarm (°C) |
| `temp_alarmlow_treshold` | 19.5 | Low temperature alarm (°C) |
| `Temp_Update_Interval_LIVE_mins` | 20 | Real sensor read interval (min) |
| `Temp_Update_Interval_SIM_mins` | 1 | Simulated sensor read interval (min) |
| `backupInterval_mins` | 240 | Data backup interval (min) |
| `maxcooling_mins` | 180 | Max cooling duration before auto-shutoff |
| `simulateSensor` | true | Use simulated temperature sensor |
| `emailme` | true | Enable email notifications |
| `skipmail` | true | Skip actual SMTP sending (log only) |
| `serialout` | true | Enable verbose logging |
| `measure` | true | Enable temperature measurement |
| `pwmfrequency` | 1000 | PWM frequency (Hz) |
| `weeklyReport_tm_wday` | 5 | Weekly report day (0=Sun, 5=Fri) |
| `weeklyReport_tm_hour` | 22 | Weekly report hour |
| `weeklyReport_tm_min` | 0 | Weekly report minute |

Email credentials are loaded from `.env` (not stored in settings pickle):

```
SMTP_HOST=smtp.gmail.com
SMTP_PORT=465
SMTP_AUTH_EMAIL=your-email@gmail.com
SMTP_AUTH_PASSWORD=your-app-password
RECIPIENT_EMAIL=recipient@example.com
NTP_SERVER=fritz.box
SECRET_KEY=change-this-to-random-string
```

## Quick Start

```bash
# Create and activate virtual environment
python -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install -r requirements.txt

# Copy and edit credentials
cp .env.example .env
# Edit .env with your SMTP credentials

# Run the server
uvicorn app.main:app --host 0.0.0.0 --port 8080

# Run tests
python -m pytest tests/ -v
```

## Running on RPi Zero

1. Enable 1-Wire interface: `sudo raspi-config` → Interface Options → 1-Wire → Enable
2. Install gpiozero: `pip install gpiozero` (or `RPi.GPIO`)
3. Wire DS18B20 to GPIO7 (with 4.7kΩ pull-up)
4. Wire relays with active-LOW logic (relay modules typically work this way)
5. Run as a systemd service for auto-start on boot

## Temperature Data Storage

- Circular buffer of 700 entries (≈1 week at 20-min intervals)
- Each entry: 4-byte Unix timestamp + 1-byte temperature deviation from 25°C (×10)
- CSV export with UTF-8 BOM, semicolon delimiter, German decimal comma
- Persisted in `data/temperature.pickle` at backup intervals (not on every reading)

## Test Status

**38 tests passing** ✅

```
tests/test_config.py       5 passed
tests/test_crud.py         7 passed
tests/test_fader.py        6 passed
tests/test_gpio.py         6 passed
tests/test_models.py       4 passed
tests/test_temperature.py  4 passed
tests/test_time_utils.py   6 passed
```

## License

See [LICENSE](LICENSE).