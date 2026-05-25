"""FastAPI Application - Main entry point for AquaControl on Raspberry Pi Zero.

Replaces the ESP8266 HTTP server with FastAPI + uvicorn.
Provides REST API + serves static frontend files."""

import asyncio
import logging
import os
from contextlib import asynccontextmanager
from datetime import datetime

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse, PlainTextResponse
from fastapi.staticfiles import StaticFiles

from app.config import get_settings, settings_to_dict, update_settings_from_dict
from app.models import (
    ScheduleEntryIn, ScheduleEntryOut, DeviceStateRequest,
    StatusResponse, ParameterUpdateRequest,
)
from app.gpio_interface import (
    get_gpio, LIGHT_PWMPIN, COOLING_PWMPIN,
    RELAYLIGHT_PIN, RELAYCO2_PIN, RELAYMOON_PIN,
    LIGHT_LEVELS, MockGPIOController,
)
from app.fader import PWMFader
from app.scheduler import Scheduler
from app.temperature import TemperatureController
from app.email_sender import EmailSender
from app.crud import get_crud, ScheduleEntry
from app.time_utils import get_formatted_date, get_esp_day_of_week, adjust_for_dst


def _mask_password(value: str) -> str:
    """Mask a password showing only first 3 and last 3 characters."""
    if len(value) <= 6:
        return value
    return value[:3] + "..." + value[-3:]

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger(__name__)

# Global state
temp_ctrl = TemperatureController()
light_fader = PWMFader()
cooling_fader = PWMFader()
gpio = get_gpio()
scheduler = Scheduler(gpio, light_fader, cooling_fader)
emailer = EmailSender()

# Background loop control
_background_task: asyncio.Task | None = None
_run_background = True
_prev_minute = -1


async def background_loop():
    """Background loop running every ~1 second - port of ESP8266 loop()."""
    global _prev_minute
    logger.info("[LOOP] Background loop started")
    get_crud().load_all()

    # Restore schedule on startup
    now = datetime.now()
    scheduler.set_outputs_according_to_schedule(now.hour, now.minute)

    # Send reboot email if enabled
    if get_settings().emailme:
        emailer.send_email(is_reboot=True)

    while _run_background:
        try:
            now = datetime.now()
            current_minute = now.hour * 60 + now.minute

            # --- Every second ---
            # Update PWM faders
            scheduler.update_pwm_outputs()

            # --- Temperature measurement every Temp_Update_Interval ---
            settings = get_settings()
            update_interval = (
                settings.Temp_Update_Interval_SIM_mins
                if settings.simulateSensor
                else settings.Temp_Update_Interval_LIVE_mins
            )
            if current_minute % update_interval == 0 and current_minute != _prev_minute:
                temp = temp_ctrl.read()
                crud = get_crud()
                crud.add_temperature_entry(int(datetime.now().timestamp()), temp)

                # Check temperature alarms (use >/< like ESP8266, not >=/<=)
                if settings.emailme and (temp > settings.temp_alarmhigh_treshold or temp < settings.temp_alarmlow_treshold):
                    emailer.send_alarm(temp)
                logger.info(f"[LOOP] Temperature: {temp:.1f}°C")

            # --- Check schedule every minute ---
            if current_minute != _prev_minute:
                _prev_minute = current_minute
                scheduler.check_schedule(now.hour, now.minute)
                scheduler.check_max_cooling_time()
                emailer.reset_weekly_flag()

                # Weekly report
                # ESP8266 uses tm_wday (Sunday=0), Python uses Monday=0.
                # Use get_esp_day_of_week() to match the ESP8266 format.
                if settings.emailme and not settings.skipmail:
                    if (get_esp_day_of_week() == settings.weeklyReport_tm_wday and
                            now.hour == settings.weeklyReport_tm_hour and
                            now.minute == settings.weeklyReport_tm_min):
                        emailer.send_weekly_report()

                # Off-all at 23:00
                if now.hour == 23 and now.minute == 0:
                    scheduler.off_all()

            # --- Backup (every backupInterval_mins) ---
            if current_minute % settings.backupInterval_mins == 0 and current_minute != 0:
                get_crud().save_all()

            await asyncio.sleep(1)

        except Exception as e:
            logger.error(f"[LOOP] Error: {e}", exc_info=True)
            await asyncio.sleep(5)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Application lifespan: start/stop background loop."""
    global _background_task
    _background_task = asyncio.create_task(background_loop())
    yield
    global _run_background
    _run_background = False
    if _background_task:
        _background_task.cancel()
        try:
            await _background_task
        except asyncio.CancelledError:
            pass
    gpio.cleanup()
    get_crud().save_all()


# Create FastAPI app
static_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "static")
app = FastAPI(
    title="AquaControl",
    description="Raspberry Pi Zero Aquarium Controller - ported from ESP8266",
    version="1.0.0",
    lifespan=lifespan,
)

# Mount static files
if os.path.isdir(static_dir):
    app.mount("/static", StaticFiles(directory=static_dir), name="static")


# ========== REST API Endpoints ==========

# ========== HTML Page Routes (matching original ESP8266) ==========

def _read_static_file(filename: str) -> str:
    """Read a static HTML file and return its content."""
    path = os.path.join(static_dir, filename)
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except FileNotFoundError:
        logger.warning(f"Static file not found: {path}")
        return f"<html><body><h1>404 - {filename} not found</h1></body></html>"


@app.get("/", response_class=HTMLResponse)
async def root():
    """Serve main navigation hub (index.html)."""
    return HTMLResponse(content=_read_static_file("index.html"))


@app.get("/light", response_class=HTMLResponse)
async def light_page():
    """Serve light/device control page (port of html4light.h)."""
    return HTMLResponse(content=_read_static_file("light.html"))


@app.get("/schedule", response_class=HTMLResponse)
async def schedule_page():
    """Serve schedule editor page (port of htmltemplate.h)."""
    return HTMLResponse(content=_read_static_file("schedule.html"))


@app.get("/settings", response_class=HTMLResponse)
async def settings_page():
    """Serve parameter settings page (port of htmlsettings.h)."""
    return HTMLResponse(content=_read_static_file("settings.html"))


@app.get("/info", response_class=HTMLResponse)
async def info_page():
    """Serve temperature history chart page (port of temperaturesvgpage.h)."""
    return HTMLResponse(content=_read_static_file("temperature.html"))


@app.get("/email", response_class=HTMLResponse)
async def email_page():
    """Send an email (reboot notification) and show status."""
    try:
        result = emailer.send_email(is_reboot=False)
        if result:
            return HTMLResponse("<html><body><h1>Email sent successfully</h1><a href='/'>Back</a></body></html>")
        else:
            return HTMLResponse("<html><body><h1>Email not sent (check settings or skipmail)</h1><a href='/'>Back</a></body></html>")
    except Exception as e:
        logger.error(f"[EMAIL] Error: {e}")
        return HTMLResponse(f"<html><body><h1>Email error: {e}</h1><a href='/'>Back</a></body></html>")


@app.get("/ram", response_class=HTMLResponse)
async def ram_page():
    """Show RAM info (replacement for ESP8266.getFreeHeap())."""
    try:
        import psutil
        mem = psutil.virtual_memory()
        info = (
            f"<h1>System RAM Info</h1>"
            f"<p>Total: {mem.total // (1024*1024)} MB</p>"
            f"<p>Available: {mem.available // (1024*1024)} MB</p>"
            f"<p>Used: {mem.used // (1024*1024)} MB</p>"
            f"<p>Percent: {mem.percent}%</p>"
            f"<a href='/'>Back</a>"
        )
        return HTMLResponse(f"<html><body>{info}</body></html>")
    except ImportError:
        return HTMLResponse("<html><body><h1>psutil not installed - install with: pip install psutil</h1><a href='/'>Back</a></body></html>")




@app.get("/api/status", response_model=StatusResponse)
async def api_status():
    """Get current system status."""
    now = datetime.now()
    temp = temp_ctrl.current if not get_settings().simulateSensor else temp_ctrl.read()

    # Read GPIO states
    pwm_light = light_fader.brightness
    pwm_cooling = cooling_fader.brightness

    if isinstance(gpio, MockGPIOController):
        relay_light = 1 if gpio.get_digital(RELAYLIGHT_PIN) else 0
        relay_co2 = 1 if gpio.get_digital(RELAYCO2_PIN) else 0
        relay_moon = 1 if gpio.get_digital(RELAYMOON_PIN) else 0
    else:
        relay_light = 1 if gpio.get_digital(RELAYLIGHT_PIN) else 0
        relay_co2 = 1 if gpio.get_digital(RELAYCO2_PIN) else 0
        relay_moon = 1 if gpio.get_digital(RELAYMOON_PIN) else 0

    return StatusResponse(
        time=now.strftime("%H:%M:%S"),
        date=get_formatted_date(),
        temp=temp,
        LIGHTPWM=pwm_light,
        COOLINGPWM=pwm_cooling,
        RELAYLIGHT=relay_light,
        RELAYCO2=relay_co2,
        RELAYMOON=relay_moon,
    )


@app.get("/api/parameters")
async def api_get_parameters():
    """Get all system parameters (passwords masked)."""
    params = settings_to_dict()
    # Mask sensitive fields
    for key in ("smtp_AUTH_PASSWORD",):
        if key in params:
            params[key] = _mask_password(params[key])
    return params


@app.post("/api/parameters")
async def api_update_parameters(data: ParameterUpdateRequest):
    """Update system parameters."""
    data_dict = {k: v for k, v in data.model_dump().items() if v is not None}
    update_settings_from_dict(data_dict)
    get_crud().save_all()
    return {"status": "ok", "parameters": settings_to_dict()}


@app.get("/api/schedule", response_model=list[ScheduleEntryOut])
async def api_get_schedule():
    """Get schedule entries."""
    entries = get_crud().get_schedule()
    return [
        ScheduleEntryOut(
            hour=e.hour, minute=e.minute,
            brightness=e.brightness,
            fadeDuration=e.fadeMinutes,
            device=e.device,
        )
        for e in entries
    ]


@app.post("/api/schedule")
async def api_update_schedule(entries: list[ScheduleEntryIn]):
    """Update schedule entries."""
    get_crud().update_schedule([e.model_dump() for e in entries])
    return {"status": "ok", "count": len(entries)}


@app.post("/api/device")
async def api_set_device(data: DeviceStateRequest):
    """Manually set a device to a specific value.
    Port of ESP8266 handleSetDeviceState() - value is raw PWM (0-1023) or on/off for relays."""
    device = data.device
    value = data.value

    if device == 0:  # PWM Light - raw value 0-1023, 500ms fade (matching ESP8266)
        light_fader.fade(value, 500)
    elif device == 1:  # PWM Cooling - raw value 0-1023, 500ms fade (matching ESP8266)
        cooling_fader.fade(value, 500)
    elif device == 2:  # Relay Light (active-LOW)
        scheduler.set_relay(RELAYLIGHT_PIN, value > 0)
    elif device == 3:  # Relay CO2 (active-LOW)
        scheduler.set_relay(RELAYCO2_PIN, value > 0)
    elif device == 4:  # Relay Moon (active-LOW)
        scheduler.set_relay(RELAYMOON_PIN, value > 0)
    else:
        raise HTTPException(status_code=400, detail=f"Unknown device: {device}")

    return {"status": "ok", "device": device, "value": value}


@app.post("/api/reset")
async def api_reset():
    """Reset / clear temperature history."""
    get_crud().clear_temperature_history()
    return {"status": "ok", "message": "Temperature history cleared"}


@app.get("/api/temperature/csv")
async def api_temperature_csv():
    """Download temperature history as CSV."""
    return PlainTextResponse(
        content=get_crud().get_temperature_csv(),
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=temperatures.csv"},
    )


@app.get("/api/temperature/current")
async def api_temperature_current():
    """Get current temperature reading."""
    temp = temp_ctrl.read()
    return {"temperature": temp}


@app.get("/api/light-levels")
async def api_light_levels():
    """Get available light levels."""
    return {"levels": LIGHT_LEVELS, "count": len(LIGHT_LEVELS)}


@app.get("/restart", response_class=HTMLResponse)
async def restart_page():
    """Save data and restart (port of ESP8266 handleRestart)."""
    get_crud().save_all()
    try:
        import subprocess
        subprocess.Popen(["sudo", "reboot"])
        return HTMLResponse("<html><body><h1>Aquacontrol wird neu gestartet...</h1><p>Seite in 60 Sekunden neu laden.</p></body></html>")
    except Exception as e:
        logger.error(f"[RESTART] Error: {e}")
        return HTMLResponse(f"<html><body><h1>Restart fehlgeschlagen: {e}</h1><a href='/'>Back</a></body></html>")


@app.get("/save", response_class=HTMLResponse)
async def save_page():
    """Manually save all data to disk (port of ESP8266 handlesaveTemperatureToEEPROM)."""
    get_crud().save_all()
    return HTMLResponse("<html><body><h1>Messdaten gespeichert.</h1><a href='/'>Back</a></body></html>")


# Run with: uvicorn app.main:app --host 0.0.0.0 --port 8080
if __name__ == "__main__":
    import uvicorn
    uvicorn.run("app.main:app", host="0.0.0.0", port=8080, reload=False)
