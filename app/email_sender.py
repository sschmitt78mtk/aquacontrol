"""Email sender module - port of ESP8266 email logic.

Uses stdlib smtplib instead of ESP_Mail_Client.
Supports reboot notifications, temperature alarms, and weekly reports."""

import smtplib
import logging
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from email.mime.base import MIMEBase
from email import encoders
from datetime import datetime

from app.config import get_settings, load_credentials
from app.crud import get_crud

logger = logging.getLogger(__name__)


class EmailSender:
    """Handles SMTP email sending with CSV attachments."""

    def __init__(self):
        self._alarm_sent_day: int = 0
        self._weekly_email_sent: bool = False
        self._weekly_sent_day: int = -1

    def send_email(self, is_reboot: bool = False, is_alarm: bool = False) -> bool:
        """Send an email with temperature CSV attachment."""
        settings = get_settings()
        if settings.skipmail:
            logger.info("[EMAIL] skipmail=True - not sending")
            return True

        today = datetime.now().day
        if is_alarm:
            if self._alarm_sent_day == today:
                logger.info("[EMAIL] Alarm already sent today - skipping")
                return False

        try:
            creds = load_credentials()
            msg = MIMEMultipart()
            smtp_email: str = str(creds["smtp_auth_email"])
            recipient: str = str(creds["recipient_email"])
            smtp_host: str = str(creds["smtp_host"])
            smtp_port: int = int(creds["smtp_port"])
            smtp_password: str = str(creds["smtp_auth_password"])

            msg["From"] = smtp_email
            msg["To"] = recipient

            if is_reboot:
                msg["Subject"] = "Neustart - Temperaturdaten"
                body = "Angehängte Temperaturdaten nach Neustart"
            elif is_alarm:
                msg["Subject"] = "Alarm Temperatur zu hoch oder zu niedrig"
                body = "Temperatur zu hoch oder zu niedrig"
                msg["X-Priority"] = "1"
            else:
                msg["Subject"] = "Wöchentlicher Temperaturreport"
                body = "Angehängte Temperaturdaten"

            msg.attach(MIMEText(body, "plain", "utf-8"))

            # Attach CSV
            csv_data = get_crud().get_temperature_csv()
            attachment = MIMEBase("text", "csv")
            attachment.set_payload(csv_data.encode("utf-8"))
            encoders.encode_base64(attachment)
            attachment.add_header(
                "Content-Disposition",
                "attachment",
                filename="temperatures.csv"
            )
            msg.attach(attachment)

            # Send via SMTP_SSL
            with smtplib.SMTP_SSL(host=smtp_host, port=smtp_port, timeout=30) as server:
                server.login(smtp_email, smtp_password)
                server.send_message(msg)

            logger.info("[EMAIL] Sent successfully")

            if is_alarm:
                self._alarm_sent_day = today
            if is_reboot:
                self._weekly_email_sent = True
                self._weekly_sent_day = datetime.now().weekday()

            return True

        except Exception as e:
            logger.error(f"[EMAIL] Error sending: {e}")
            return False

    def send_weekly_report(self) -> bool:
        """Send weekly temperature report (only once per week)."""
        today = datetime.now().weekday()
        if self._weekly_email_sent and self._weekly_sent_day == today:
            return False
        result = self.send_email(is_reboot=False)
        if result:
            self._weekly_email_sent = True
            self._weekly_sent_day = today
        return result

    def send_alarm(self, temp: float) -> bool:
        """Send temperature alarm email."""
        return self.send_email(is_alarm=True)

    def reset_weekly_flag(self):
        """Reset weekly sent flag (called when day changes)."""
        now = datetime.now()
        if self._weekly_sent_day != now.weekday():
            self._weekly_email_sent = False
