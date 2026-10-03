"""Tests for NTP startup validation, manual system-time setting and clock status."""

import asyncio
import logging
import subprocess
from unittest.mock import Mock, patch

import pytest

from app.main import get_time_status, is_ntp_synchronized, set_system_time
from app.models import TimeStatusResponse


def test_ntp_synchronized_when_timedatectl_returns_yes():
    completed = Mock(returncode=0, stdout="yes\n")
    with patch("app.main.subprocess.run", return_value=completed) as run:
        assert is_ntp_synchronized() is True

    assert run.call_args.args[0] == [
        "timedatectl", "show", "--property=NTPSynchronized", "--value",
    ]


def test_ntp_not_synchronized_for_no_or_unavailable_timedatectl():
    with patch("app.main.subprocess.run", return_value=Mock(returncode=0, stdout="no\n")):
        assert is_ntp_synchronized() is False
    with patch("app.main.subprocess.run", side_effect=FileNotFoundError):
        assert is_ntp_synchronized() is False



def test_simulate_invalid_time_forces_fish_safe_startup():
    from app import main

    with patch("app.main.SIMULATE_INVALID_TIME", True), \
         patch("app.main.is_ntp_synchronized") as ntp_check, \
         patch.object(main, "get_crud") as get_crud, \
         patch.object(main.scheduler, "start_fish_safe_mode") as start_fish_safe:
        main.initialize_startup_mode()

    assert main._fish_safe_mode is True
    ntp_check.assert_not_called()
    get_crud.return_value.load_all.assert_called_once_with()
    start_fish_safe.assert_called_once_with()


def test_set_system_time_disables_ntp_sets_time_and_reenables_ntp():
    with patch("app.main.subprocess.run", return_value=Mock(returncode=0)) as run:
        set_system_time("2026-08-10T14:35")

    assert [call.args[0] for call in run.call_args_list] == [
        ["sudo", "/usr/bin/timedatectl", "set-ntp", "false"],
        ["sudo", "/usr/bin/timedatectl", "set-time", "2026-08-10 14:35:00"],
        ["sudo", "/usr/bin/timedatectl", "set-ntp", "true"],
    ]


def test_set_system_time_reenables_ntp_when_set_time_fails():
    calls = []

    def fake_run(command, **kwargs):
        calls.append(command)
        if "set-ntp" in command:
            return Mock(returncode=0, stdout="", stderr="")
        return Mock(
            returncode=1,
            stdout="",
            stderr="Failed to set time: Automatic time synchronization is enabled",
        )

    with patch("app.main.subprocess.run", side_effect=fake_run):
        with pytest.raises(RuntimeError, match="Automatic time synchronization is enabled"):
            set_system_time("2026-08-10T14:35")

    assert calls == [
        ["sudo", "/usr/bin/timedatectl", "set-ntp", "false"],
        ["sudo", "/usr/bin/timedatectl", "set-time", "2026-08-10 14:35:00"],
        ["sudo", "/usr/bin/timedatectl", "set-ntp", "true"],
    ]


@pytest.mark.parametrize("value", ["", "2023-12-31T23:59", "2101-01-01T00:00", "not-a-time"])
def test_set_system_time_rejects_invalid_values_without_running_command(value):
    with patch("app.main.subprocess.run") as run:
        with pytest.raises(ValueError):
            set_system_time(value)
    run.assert_not_called()


def test_set_system_time_reports_timedatectl_failure():
    completed = Mock(returncode=1, stdout="", stderr="not permitted")
    with patch("app.main.subprocess.run", return_value=completed):
        with pytest.raises(RuntimeError, match="not permitted"):
            set_system_time("2026-08-10T14:35")

    with patch("app.main.subprocess.run", side_effect=subprocess.TimeoutExpired("timedatectl", 10)):
        with pytest.raises(subprocess.TimeoutExpired):
            set_system_time("2026-08-10T14:35")


def test_auto_recover_returns_true_when_ntp_available_after_interval():
    from app import main
    with patch.object(main, "_last_ntp_check", 0.0), \
         patch("app.main.is_ntp_synchronized", return_value=True) as ntp_check:
        result = main._should_auto_recover_from_fish_safe(30.0)
        assert result is True
        assert main._last_ntp_check == 30.0
    ntp_check.assert_called_once_with()


def test_auto_recover_skips_ntp_check_within_recheck_interval():
    from app import main
    with patch.object(main, "_last_ntp_check", 0.0), \
         patch("app.main.is_ntp_synchronized") as ntp_check:
        assert main._should_auto_recover_from_fish_safe(29.9) is False
    ntp_check.assert_not_called()


def test_auto_recover_disabled_when_simulate_invalid_time():
    from app import main
    with patch.object(main, "_last_ntp_check", 0.0), \
         patch("app.main.SIMULATE_INVALID_TIME", True), \
         patch("app.main.is_ntp_synchronized") as ntp_check:
        assert main._should_auto_recover_from_fish_safe(100.0) is False
    ntp_check.assert_not_called()


def test_exit_fish_safe_mode_resumes_normal_schedule():
    from app import main
    main._fish_safe_mode = True
    try:
        with patch.object(main.scheduler, "stop_fish_safe_mode") as stop, \
             patch.object(main.scheduler, "set_outputs_according_to_schedule") as set_outputs, \
             patch.object(main, "get_settings") as get_settings, \
             patch.object(main.emailer, "send_email") as send_email:
            get_settings.return_value.emailme = True
            main._exit_fish_safe_mode()
        stop.assert_called_once_with()
        set_outputs.assert_called_once()
        send_email.assert_called_once_with(is_reboot=True)
        assert main._fish_safe_mode is False
    finally:
        main._fish_safe_mode = False


def test_get_time_status_parses_synchronized_ntp_and_timezone():
    completed = Mock(
        returncode=0,
        stdout="NTPSynchronized=yes\nNTP=yes\nTimezone=Europe/Berlin\n",
    )
    with patch("app.main.subprocess.run", return_value=completed) as run:
        status = get_time_status()

    assert run.call_args.args[0] == [
        "timedatectl", "show",
        "--property=NTPSynchronized", "--property=NTP", "--property=Timezone",
    ]
    assert status["ntp_synchronized"] is True
    assert status["ntp_active"] is True
    assert status["timezone"] == "Europe/Berlin"
    assert status["local_time"]
    assert status["utc_time"]


def test_get_time_status_reports_unsynchronized_clock():
    completed = Mock(returncode=0, stdout="NTPSynchronized=no\nNTP=no\nTimezone=UTC\n")
    with patch("app.main.subprocess.run", return_value=completed):
        status = get_time_status()

    assert status["ntp_synchronized"] is False
    assert status["ntp_active"] is False
    assert status["timezone"] == "UTC"


def test_get_time_status_falls_back_safely_when_timedatectl_is_unavailable():
    with patch("app.main.subprocess.run", side_effect=FileNotFoundError):
        unavailable = get_time_status()
    assert unavailable["ntp_synchronized"] is False
    assert unavailable["ntp_active"] is False
    assert unavailable["timezone"] == ""
    assert unavailable["local_time"]
    assert unavailable["utc_time"]

    with patch("app.main.subprocess.run", return_value=Mock(returncode=1, stdout="", stderr="boom")):
        failed = get_time_status()
    assert failed["ntp_synchronized"] is False
    assert failed["ntp_active"] is False


def test_api_time_status_returns_model_fields():
    from app import main

    status = {
        "ntp_synchronized": True,
        "ntp_active": True,
        "local_time": "2026-08-10 14:35:00",
        "utc_time": "2026-08-10 12:35:00",
        "timezone": "Europe/Berlin",
    }
    with patch.object(main, "get_time_status", return_value=status) as get_status:
        response = asyncio.run(main.api_time_status())

    get_status.assert_called_once_with()
    assert isinstance(response, TimeStatusResponse)
    assert response.model_dump() == status


def test_initialize_startup_mode_logs_untrusted_time_source(caplog):
    from app import main

    try:
        with caplog.at_level(logging.WARNING):
            with patch("app.main.SIMULATE_INVALID_TIME", True), \
                 patch.object(main, "get_crud"), \
                 patch.object(main.scheduler, "start_fish_safe_mode"):
                main.initialize_startup_mode()
        assert "[TIME] Clock untrusted" in caplog.text
    finally:
        main._fish_safe_mode = False


def test_initialize_startup_mode_logs_trusted_time_source(caplog):
    from app import main

    with caplog.at_level(logging.INFO):
        with patch("app.main.SIMULATE_INVALID_TIME", False), \
             patch("app.main.is_ntp_synchronized", return_value=True), \
             patch.object(main, "get_crud"), \
             patch.object(main.scheduler, "set_outputs_according_to_schedule"), \
             patch.object(main, "get_settings") as get_settings:
            get_settings.return_value.send_email = False
            main.initialize_startup_mode()

    assert "[TIME] Clock trusted" in caplog.text
