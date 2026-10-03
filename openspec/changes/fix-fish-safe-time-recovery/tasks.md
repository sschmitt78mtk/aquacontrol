# Tasks

## 1. Manual clock recovery (fish-safe `/set-time`)

- [x] 1.1 In `app/main.py`, add a private `_run_timedatectl(*args)` helper that runs `sudo /usr/bin/timedatectl …` via `subprocess.run` (`capture_output=True`, `text=True`, `check=False`, bounded `timeout`) and returns the result; verify `python -m pytest tests/test_time_startup.py -v` still passes with the helper in place.
- [x] 1.2 Rewrite `set_system_time(value)` to validate the input first (unchanged), then run `set-ntp false` -> `set-time` -> `set-ntp true`, with the re-enable in a `finally` block and the original error re-raised on failure; verify via the tests added in 1.3.
- [x] 1.3 Add tests to `tests/test_time_startup.py` asserting the ordered `timedatectl` arguments for the happy path, that `set-ntp true` still runs when `set-time` fails, and that invalid input calls `subprocess.run` zero times; verify `python -m pytest tests/test_time_startup.py -v` is green.
- [x] 1.4 Update `README.md`: correct the manual-time description and extend the restricted sudoers example to `set-time *` plus `set-ntp true`/`set-ntp false`; verify the documented verbs exactly match the calls made by `set_system_time`.

## 2. Clock / time-source status API

- [x] 2.1 Add `get_time_status()` in `app/main.py` that parses one `timedatectl show --property=NTPSynchronized --property=NTP --property=Timezone` call and adds local/UTC times; verify with the unit test added in 2.3.
- [x] 2.2 Add a `TimeStatusResponse` model in `app/models.py` and a `GET /api/time-status` handler returning it; verify by invoking the handler with `get_time_status` patched and confirming the response fields match the model.
- [x] 2.3 Add tests for `get_time_status()` in `tests/test_time_startup.py` (parses `NTPSynchronized=yes/no`, reports NTP active, and reports a safe fallback when `timedatectl` is unavailable); verify `python -m pytest tests/test_time_startup.py -v` is green.
- [x] 2.4 Log the selected time source once in `initialize_startup_mode()` with a `[TIME]` tag (trusted clock vs. fish-safe); verify the line appears in `uvicorn` output on startup.

## 3. Settings page display

- [x] 3.1 Add a status block to `static/settings.html` that fetches `/api/time-status` on load and shows NTP synchronized / NTP active / local time / UTC time / timezone with German labels; verify by opening `/settings` and confirming the displayed values match the endpoint output.

## 4. Integration verification

- [x] 4.1 Run `python -m pytest tests/ -v` and confirm the whole suite is green.
- [ ] 4.2 On a host with an NTP service active, submit the `/set-time` form and confirm the clock is set, `timedatectl show --property=NTP --value` is `yes` afterwards, and the app leaves fish-safe mode and restores the schedule.
