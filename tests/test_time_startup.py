"""Tests for NTP startup validation and manual system-time setting."""

import subprocess
from unittest.mock import Mock, patch

import pytest

from app.main import is_ntp_synchronized, set_system_time


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


def test_set_system_time_calls_restricted_timedatectl_command():
    with patch("app.main.subprocess.run", return_value=Mock(returncode=0)) as run:
        set_system_time("2026-08-10T14:35")

    assert run.call_args.args[0] == [
        "sudo", "/usr/bin/timedatectl", "set-time", "2026-08-10 14:35:00",
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
