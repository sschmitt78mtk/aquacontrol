# Design

## Context

See `proposal.md` - Why. Current state that shapes the approach:

- Fish-safe mode already exists: `app/main.py` decides it in `initialize_startup_mode()`
  (`SIMULATE_INVALID_TIME or not is_ntp_synchronized()`), gates routes with the
  `fish_safe_time_gate` middleware, and auto-recovers every 30 s in `background_loop()`.
  `app/scheduler.py` owns the emergency outputs (`start_fish_safe_mode`,
  `update_fish_safe_mode`).
- Manual recovery is `set_system_time(value)` in `app/main.py`; it validates the
  `datetime-local` string, then runs `sudo /usr/bin/timedatectl set-time "…"`.
- `is_ntp_synchronized()` shells out to `timedatectl show --property=NTPSynchronized --value`.
- Constraints already in the project: hardware/OS must stay mockable (tests patch
  `app.main.subprocess.run`), keep the ~1 s loop cheap, preserve ESP8266 behavior, German UI.

The blocking fact (verified against the systemd source): `timedated`'s `SetTime` refuses with
`BUS_ERROR_AUTOMATIC_TIME_SYNC_ENABLED` ("Automatic time synchronization is enabled") whenever
any listed NTP unit is active — `context_ntp_service_is_active(c) > 0`. `systemd-timesyncd` is
active by default on Raspberry Pi OS, so `set-time` is rejected even when NTP never synced.

## Goals / Non-Goals

**Goals:**
- Manual time entry succeeds while an NTP service is running, and leaves NTP active afterwards.
- The operator can see whether the running clock is trustworthy (NTP synced / service active /
  local + UTC time / timezone), both in the UI and in the startup log.
- Stay within the project's testability pattern: no new dependency, `subprocess.run` stays the
  single OS seam, tests stay hardware-free.

**Non-Goals:**
- Changing fish-safe output behavior (CO2/PWM off, cooling left off, light 10 h/24 h) - accepted
  as-is; fish-safe intentionally offers no "advanced" functionality.
- RTC support (the Pi Zero has none).
- Persisting fish-safe state across reboots (brownout loops accepted).
- Granting broader sudo privileges than the two `timedatectl` verbs needed.

## Decisions

### D1: Bracket the clock write with `set-ntp false` / `set-ntp true`

`set_system_time()` performs, in order:

```
 validate input                       (no privilege used)
      |
      v
 sudo timedatectl set-ntp false        <- NTP service must be inactive for SetTime
      |
      v
 sudo timedatectl set-time "<local>"   <- timedated now accepts the write
      |
      v
 sudo timedatectl set-ntp true         <- restore auto-correction (in try/finally)
```

Rationale: stays on the systemd-blessed `timedatectl` surface the code already uses, keeps the
privilege list tiny (two verbs), and re-enabling NTP lets the clock self-correct if
connectivity returns.

Alternatives considered:
- `sudo date -s "…"` - bypasses the guard without touching NTP, but needs a different, sharper
  sudo rule and leaves the clock at the mercy of a running timesyncd.
- `systemctl stop/start <unit>` - forces a hardcoded unit name (timesyncd vs chrony vs
  NetworkManager), leaking host specifics into the app.
- Leave NTP disabled after the manual set - rejected: the clock would never auto-correct and a
  later NTP-capable network would not help.

### D2: Re-enable NTP in `finally`; keep one entry point

`set_system_time(value)` keeps its signature and its "raise on failure" contract so the
`/set-time` POST handler (`main.py`) still renders the reason. The body becomes:

- validate first (unchanged, so bad input never reaches a privileged call);
- `set-ntp false`; if that fails, raise (nothing to restore);
- `set-time`; if it fails, let `finally` re-enable NTP and raise the error;
- `finally`: `set-ntp true`, logging at ERROR if the re-enable itself fails.

A small private helper (e.g. `_run_timedatectl(*args)`) wraps `subprocess.run` with
`sudo /usr/bin/timedatectl …`, `capture_output=True`, `check=False`, and a bounded `timeout`,
so all three calls share one seam and tests can patch a single function.

### D3: Extend the restricted sudoers rule

The host rule must allow the two new verbs, still nothing else:

```
sascha ALL=(root) NOPASSWD: /usr/bin/timedatectl set-time *, \
                             /usr/bin/timedatectl set-ntp true, \
                             /usr/bin/timedatectl set-ntp false
```

Rationale: `set-ntp` only takes a boolean, so the rule stays narrow. Documented in `README.md`.

### D4: Read clock status through one `timedatectl show` call

Add `get_time_status()` returning a small dict (or model) built from a single
`timedatectl show --property=NTPSynchronized --property=NTP --property=Timezone` call, plus
local time from `datetime.now()` and UTC time from `datetime.now(timezone.utc)`. Expose it as
`GET /api/time-status` (a Pydantic response model in `app/models.py`, matching the other
`/api/*` endpoints) and render it as a small status block on `static/settings.html`, which
already fetches `/api/parameters` on load - same pattern, no new framework.

Rationale: one subprocess call, on demand only (page open / endpoint hit), so the ~1 s loop
stays unchanged and cheap on the Pi Zero. Also log the time source once in
`initialize_startup_mode()` (`[TIME] …`) so the journal reflects it.

### D5: Tests patch the single subprocess seam

New tests in `tests/test_time_startup.py` patch `app.main.subprocess.run` (the existing
pattern) and assert the ordered `timedatectl` arguments for the happy path, that NTP is
re-enabled when `set-time` fails, and that bad input never calls `subprocess.run` at all.

## Risks / Trade-offs

- [`set-ntp false` persists and disables the NTP unit; if the process dies between `false` and
  `true`, NTP stays disabled across reboots] -> always restore in `finally`; log remediation at
  ERROR when the restore fails. Accepted residual risk, called out in the README.
- [Re-enabling NTP immediately can let timesyncd step the clock if a server is reachable] ->
  desirable: it corrects drift; the just-set value is only a starting point.
- [Extra subprocess calls on the request path add a little latency] -> three short, bounded
  calls only during a manual time entry; time-status is one call on demand.
- [Settings-page status is unreachable while fish-safe mode is active (routes redirect to
  `/set-time`)] -> intended; the block reflects normal-operation status, which is where drift
  matters. Surfacing fish-safe state would require widening the middleware gate (out of scope).
- [Timezone is displayed but the input is local `datetime-local` with no timezone] -> unchanged
  behavior; the operator enters local time, consistent with today.

## Migration Plan

1. Merge the code and README changes.
2. On the host, update `/etc/sudoers.d/aquacontrol-time` with `README.md`'s rule and verify mode
   `0440` (`sudo visudo -f /etc/sudoers.d/aquacontrol-time`).
3. Restart the service and confirm the Settings page shows the clock status.
4. Rollback: revert the code; the widened sudoers rule is harmless if left, but should be
   reverted with the app for least privilege.
