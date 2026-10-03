# Proposal

## Why

After a power failure the Raspberry Pi Zero has no valid clock, so AquaControl starts in
fish-safe mode: the main light runs a 10 h/24 h emergency cycle, CO2 and PWM stay off, and
every route redirects to `/set-time`. Recovery depends on either NTP becoming reachable or an
operator setting the clock on that page — but the manual path can never succeed, because
systemd refuses `timedatectl set-time` while any NTP service is active, and `systemd-timesyncd`
is active by default on Raspberry Pi OS even when it has never reached a server. An operator
without internet is therefore locked in fish-safe mode indefinitely, and the README's claim
that manual entry "works without Internet or NTP" is not true. There is also no way to see
whether the running clock is trustworthy.

## What Changes

- **Fix manual time entry.** `set_system_time()` disables NTP (`timedatectl set-ntp false`),
  sets the clock (`set-time`), and re-enables NTP (`set-ntp true`) — the re-enable in a
  `try/finally` so NTP is restored even when `set-time` fails. Input validation stays ahead of
  any privileged call, and failures still raise so the `/set-time` page shows the reason.
- **Extend the restricted sudoers rule** so `timedatectl set-ntp true|false` is permitted
  alongside the already allowed `timedatectl set-time *`.
- **Surface clock / time-source status.** Add a read-only `get_time_status()` helper and a
  `GET /api/time-status` endpoint (NTP synchronized, NTP service active, local time, UTC time,
  timezone), show it in a status block on the Settings page, and log the time source once at
  startup.
- **Update the README** manual-time section and its sudoers example to match the real behavior.
- **Add tests** asserting the ordered `timedatectl` call sequence and that NTP is re-enabled
  when `set-time` fails.

## Capabilities

### New Capabilities
- `time-safety`: how AquaControl decides whether the system clock is trustworthy at startup,
  what it does when it is not (fish-safe mode), how an operator recovers manually, and how the
  current clock / time-source status is reported.

### Modified Capabilities
- (none — the project has no existing specs yet)

## Impact

- **Code:** `app/main.py` (`set_system_time`, new `get_time_status`, `GET /api/time-status`,
  startup logging), `app/models.py` (new status response model), `static/settings.html`
  (status block), `tests/test_time_startup.py` (new tests).
- **Docs / deployment:** `README.md`; the host file `/etc/sudoers.d/aquacontrol-time` must be
  updated manually (requires `set-ntp true` and `set-ntp false` in addition to `set-time`).
- **Unchanged / explicitly out of scope (accepted):** fish-safe turns no cooling or temperature
  alarm on (no "advanced" behavior while the clock is unconfirmed); Raspberry Pi Zero has no
  RTC so none is supported; repeated brownout reboots may keep the light on and that is accepted.
- **Not breaking:** existing routes, payloads and the fish-safe output behavior stay the same.
