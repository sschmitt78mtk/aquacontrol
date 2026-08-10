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