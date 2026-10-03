"""Pydantic models for API request/response."""

from pydantic import BaseModel
from typing import Optional


class ScheduleEntryIn(BaseModel):
    hour: int
    minute: int
    brightness: int
    fadeDuration: int
    device: int


class ScheduleEntryOut(BaseModel):
    hour: int
    minute: int
    brightness: int
    fadeDuration: int
    device: int


class DeviceStateRequest(BaseModel):
    device: int
    value: int


class StatusResponse(BaseModel):
    time: str
    date: str
    temp: float
    LIGHTPWM: int
    COOLINGPWM: int
    RELAYLIGHT: int
    RELAYCO2: int
    RELAYMOON: int


class ParameterUpdateRequest(BaseModel):
    temp_alarmhigh_treshold: Optional[float] = None
    temp_alarmlow_treshold: Optional[float] = None
    Temp_Update_Interval_LIVE_mins: Optional[int] = None
    Temp_Update_Interval_SIM_mins: Optional[int] = None
    backupInterval_mins: Optional[int] = None
    maxcooling_mins: Optional[int] = None
    # Removed: simulateSensor, emailme, skipmail
    send_email: Optional[bool] = None
    serialout: Optional[bool] = None
    measure: Optional[bool] = None
    pwmfrequency: Optional[int] = None
    weeklyReport_tm_wday: Optional[int] = None
    weeklyReport_tm_hour: Optional[int] = None
    weeklyReport_tm_min: Optional[int] = None
    # Email settings
    smtp_AUTH_EMAIL: Optional[str] = None
    smtp_AUTH_PASSWORD: Optional[str] = None
    smtp_RECIPIENT_EMAIL: Optional[str] = None
