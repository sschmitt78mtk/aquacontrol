# Spec Delta

## Purpose

Defines how AquaControl decides whether the system clock can be trusted, what it does with the
tank outputs when it cannot, how an operator recovers to normal operation, and how the current
clock / time-source state is reported.

## ADDED Requirements

### Requirement: Clock trust at startup

At startup the system SHALL treat the system clock as trustworthy only when the operating system reports that network time (NTP) is synchronized. When the clock is not trustworthy the system MUST start in fish-safe mode; when it is trustworthy the system MUST start the normal schedule.

#### Scenario: NTP synchronized at startup
- **WHEN** the system starts and the OS reports NTP as synchronized
- **THEN** the system MUST NOT enter fish-safe mode and MUST restore device states according to the schedule for the current time

#### Scenario: NTP not synchronized at startup
- **WHEN** the system starts and the OS does not report NTP as synchronized
- **THEN** the system MUST enter fish-safe mode

### Requirement: Fish-safe route gate

While fish-safe mode is active, the system SHALL redirect every HTTP request to the time-entry page, except the time-entry page itself and the favicon.

#### Scenario: Request while fish-safe mode is active
- **WHEN** fish-safe mode is active and a client requests any route other than the time-entry page or favicon
- **THEN** the system MUST respond with a redirect to the time-entry page

#### Scenario: Time-entry page while fish-safe mode is active
- **WHEN** fish-safe mode is active and a client requests the time-entry page
- **THEN** the system MUST return the time-entry page

#### Scenario: Time-entry page in normal mode
- **WHEN** fish-safe mode is not active and a client requests the time-entry page
- **THEN** the system MUST redirect to the main page

### Requirement: Fish-safe output behavior

While fish-safe mode is active, the system SHALL hold the CO2 relay and both PWM outputs off, and MUST keep the main-light relay on for the first 10 hours of each 24-hour interval measured from when fish-safe mode started.

#### Scenario: Main light at mode start
- **WHEN** fish-safe mode starts
- **THEN** the main-light relay MUST be on

#### Scenario: Main light after ten hours
- **WHEN** ten hours have elapsed within the current 24-hour interval
- **THEN** the main-light relay MUST be off until the next interval begins at 24 hours

#### Scenario: CO2 and PWM re-forced off
- **WHEN** the CO2 relay or a PWM output is changed by any other means while fish-safe mode is active
- **THEN** the system MUST force it off again

### Requirement: Fish-safe moonlight signal

While fish-safe mode is active, the system SHALL emit a repeating SOS pattern on the moonlight relay using whole-second on/off durations.

#### Scenario: SOS pattern repeats
- **WHEN** fish-safe mode is active
- **THEN** the moonlight relay MUST follow the repeating dot-dash pattern on whole-second boundaries

### Requirement: Manual clock setting

The system SHALL let an operator set the system clock from the time-entry page while fish-safe mode is active, and this MUST succeed even while an NTP service is running. After a successful manual set the NTP service MUST be active again.

#### Scenario: Manual set while an NTP service is running
- **WHEN** the operator submits a valid local date and time while an NTP service is active
- **THEN** the system clock MUST be set to that value, the NTP service MUST be active again afterwards, and fish-safe mode MUST end

#### Scenario: Invalid input is rejected before any privileged call
- **WHEN** the operator submits a missing, malformed, or out-of-range date and time
- **THEN** the system MUST reject it without running any privileged command

#### Scenario: Privileged set fails
- **WHEN** the privileged set-time operation fails
- **THEN** the system MUST report the failure on the time-entry page and MUST leave the NTP service active

### Requirement: Automatic recovery when NTP becomes available

While fish-safe mode is active, the system SHALL re-check the NTP synchronization state periodically and MUST resume the normal schedule as soon as it becomes synchronized.

#### Scenario: NTP synchronizes after startup
- **WHEN** fish-safe mode is active and the NTP synchronization state becomes synchronized
- **THEN** the system MUST end fish-safe mode and restore device states according to the schedule for the current time

#### Scenario: NTP still not synchronized
- **WHEN** fish-safe mode is active and the NTP synchronization state is still not synchronized within the re-check interval
- **THEN** the system MUST remain in fish-safe mode and MUST NOT restore the normal schedule

### Requirement: Time-source status visibility

The system SHALL expose the current clock trust state as read-only data and display it on the settings page, including whether NTP is synchronized, whether the NTP service is active, and the current local and UTC time with timezone.

#### Scenario: Status endpoint
- **WHEN** a client requests the time-status endpoint while normal operation is active
- **THEN** the system MUST return the NTP synchronized flag, the NTP service active flag, the current local time, the current UTC time, and the timezone

#### Scenario: Settings page shows clock status
- **WHEN** an operator opens the settings page
- **THEN** the page MUST display the current clock / time-source status
