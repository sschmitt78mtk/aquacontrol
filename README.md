# AquaControl - ESP8266 Aquarium Monitoring and Control System

## This branch is working but will no longer be maintained in favour for the Raspberry-branch

## Overview

AquaControl is an Arduino-based project for ESP8266 microcontrollers that provides dual functionality for aquarium management:
- **Temperature Monitoring & Logging** with email alerts and CSV export
- **Lighting Control** with PWM dimming, scheduling, and relay management

**Important Note**: Due to RAM limitations on the ESP8266, both functions cannot be activated simultaneously. The project uses compile-time flags (`CONTROL` and `MONITOR`) to select which mode to build.

## Project Structure

### Main Files
- `aquacontrol.ino` - Main Arduino sketch with dual-mode functionality
- `timestuff.h` - Time utilities for NTP, DST adjustment, and date formatting
- `backlight_fader.hpp` / `backlight_fader.cpp` - PWM fading controller for smooth light transitions
- `htmltemplate.h` - HTML interface for lighting schedule configuration
- `html4light.h` - HTML interface for direct light/device control
- `htmlsettings.h` - HTML interface for system parameter configuration
- `temperaturesvgpage.h` - HTML interface for temperature visualization

## Hardware Requirements

### ESP8266 Pin Mapping
```
#define COOLING_OFF_PIN D0     // Water level sensor (low water detection)
#define RELAYLIGHT_PIN D1      // Main light relay
#define RELAYCO2_PIN D2        // CO2 valve relay
#define RELAYMOON_PIN LED_BUILTIN  // Moonlight relay (D4 on NodeMCU)
#define LIGHT_PWMPIN D5        // PWM-controlled lighting (10-bit resolution)
#define COOLING_PWMPIN D6      // PWM-controlled fan cooling
#define ONE_WIRE_BUS D7        // DS18B20 temperature sensor
```

### Components
- ESP8266 (NodeMCU or D1 Mini)
- DS18B20 temperature sensor (optional - can be simulated)
- 5V relays for lights, CO2, and moon lighting
- PWM-controlled LED driver
- PWM-controlled fan for cooling
- Water level sensor (optional safety feature)

## Features

### Temperature Monitoring Mode (MONITOR)
- Real-time temperature measurement (or simulation)
- Historical data logging (700 data points @ 20-minute intervals)
- CSV export with Excel-compatible format
- Email alerts for temperature thresholds
- Weekly automated email reports
- SVG temperature graph visualization
- Data persistence in EEPROM

### Lighting Control Mode (CONTROL)
- 7-level brightness presets (0-100% PWM)
- Configurable schedules with fade durations
- Multiple device control:
  - PWM Lighting (D5)
  - Cooling Fan (D6)
  - Main Light Relay (D1)
  - CO2 Valve Relay (D2)
  - Moonlight Relay (D4)
- Automatic shut-off at 23:00
- Safety timeout for cooling (max 3 hours)

## Web Interface

The project provides a comprehensive web interface accessible via the ESP8266's IP address:

### Common Pages (Both Modes)
- **Settings** (`/settings`) - System parameter configuration
- **RAM Info** (`/ram`) - Memory usage statistics
- **Restart** (`/restart`) - System reboot
- **Status** (`/status`) - JSON API for current state

### Monitoring Mode Pages
- **Temperature Graph** (`/info`) - Interactive SVG temperature visualization
- **CSV Download** (`/csv`) - Export temperature history as CSV
- **Email Test** (`/email`) - Manually trigger email report

### Control Mode Pages
- **Direct Control** (`/light`) - Manual device control with sliders
- **Schedule Editor** (`/schedule`) - Graphical schedule configuration
- **Schedule API** (`/getSchedule`, `/setSchedule`) - JSON endpoints

## Configuration

### Compile-Time Configuration
Edit `aquacontrol.ino` to set operating mode:
```cpp
#define CONTROL Yes  // Enable lighting control
#define MONITOR Yes  // Enable temperature monitoring
```

**Note**: For stable operation, enable only ONE of these flags due to ESP8266 RAM limitations.

### Runtime Parameters
All configurable parameters are accessible via the web interface:
- Temperature alarm thresholds (high/low)
- Update intervals (live/simulation)
- WiFi credentials (SSID/password)
- NTP server settings
- Email configuration (SMTP credentials)
- PWM frequency
- Weekly report schedule

## Memory Limitations

The ESP8266 has approximately 80KB of usable RAM. Running both MONITOR and CONTROL modes simultaneously causes instability due to:

1. **HTML Templates**: Large PROGMEM strings for web interfaces
2. **Temperature History**: 700-entry buffer (~3.5KB)
3. **Email Library**: ESP_Mail_Client requires significant heap
4. **Web Server**: ESP8266WebServer memory overhead
5. **JSON Processing**: ArduinoJson dynamic memory allocation

### Memory Usage Estimates
- **MONITOR Mode**: ~45-50KB RAM (with email capabilities)
- **CONTROL Mode**: ~35-40KB RAM (with schedule management)
- **Combined**: Exceeds 80KB, causing crashes or undefined behavior

## Installation

1. **Hardware Setup**
   - Connect components according to pin mapping
   - Ensure proper power supply (3.3V/5V as required)
   - Connect temperature sensor (if using real monitoring)

2. **Software Setup**
   - Install Arduino IDE with ESP8266 support
   - Install required libraries:
     - ESP8266WiFi
     - ESP8266WebServer
     - NTPClient
     - TimeLib
     - ArduinoJson
     - OneWire & DallasTemperature (MONITOR mode)
     - ESP_Mail_Client & LittleFS (MONITOR mode)

3. **Configuration**
   - Set appropriate `#define` flags in `aquacontrol.ino`
   - Upload sketch to ESP8266
   - Connect to WiFi access point
   - Access web interface to configure parameters

## EEPROM Layout

```
0-200:     Schedule entries (20 × 5 bytes)
201-510:   System parameters (future use)
512-4095:  Temperature history (700 × 5 bytes)
```

### Data Structures
```cpp
struct parameter {
  float temp_alarmhigh_treshold = 28.5;
  float temp_alarmlow_treshold = 19.5;
  uint16_t Temp_Update_Interval_LIVE_mins = 20;
  // ... additional parameters
};

struct ScheduleEntry {
    uint8_t hour;        // 0-23
    uint8_t minute;      // 0-59
    uint8_t brightness;  // 0-100
    uint8_t fadeMinutes; // 0-60
    uint8_t device;      // 0-7
};
```

## API Endpoints

### GET Endpoints
- `/status` - JSON status (time, temperature, device states)
- `/getParameters` - JSON system parameters
- `/getSchedule` - JSON lighting schedule (CONTROL mode)
- `/csv` - Temperature history as CSV (MONITOR mode)
- `/ram` - Memory usage statistics

### POST Endpoints
- `/saveParams` - Update system parameters
- `/setSchedule` - Update lighting schedule (CONTROL mode)
- `/setDeviceState` - Control individual devices

## Troubleshooting

### Common Issues

1. **WiFi Connection Failures**
   - Verify SSID/password in settings
   - Check router compatibility (2.4GHz only)
   - Ensure adequate signal strength

2. **Email Not Sending**
   - Verify SMTP credentials (use app passwords for Gmail)
   - Check "skipmail" setting in parameters
   - Monitor serial output for error messages

3. **Memory Issues**
   - Ensure only ONE mode (CONTROL or MONITOR) is enabled
   - Reduce temperature history size if needed
   - Simplify HTML templates for lower memory usage

4. **Temperature Sensor Errors**
   - Check wiring (data pin pull-up resistor)
   - Verify sensor address with OneWire scanner
   - Enable simulation mode for testing

### Serial Debug Output
Enable `serialout` parameter for detailed debugging:
- WiFi connection status
- Temperature readings
- Schedule execution
- Memory usage warnings
- Email sending status

## Development Notes

### Code Architecture
- **Modular Design**: Separate functionality into header files
- **PROGMEM Storage**: HTML templates stored in flash memory
- **EEPROM Persistence**: Critical data survives power cycles
- **Error Handling**: Graceful degradation when resources limited

### Memory Optimization Techniques
1. Use `PROGMEM` for large strings
2. Minimize global variables
3. Use `uint8_t` and `uint16_t` where appropriate
4. Dynamic memory allocation only when necessary
5. Regular `yield()` calls to prevent watchdog resets

## License

This project is available for personal and educational use. Commercial use requires permission.

## Credits

- **Libraries**: Various Arduino/ESP8266 community libraries
- **Inspiration**: Home aquarium automation needs

## Safety Warnings

1. **Electrical Safety**: Use proper enclosures and insulation for 110V/220V components
2. **Water Safety**: Ensure all electrical components are properly sealed from moisture
3. **Fire Safety**: Do not leave high-power devices unattended for extended periods
4. **Animal Safety**: Monitor temperature regularly to prevent harm to aquatic life
5. **Data Safety**: Regular backups of EEPROM data recommended

---

*Last Updated: February 2026*  
*Project Version: V4*  
*ESP8266 Compatibility: NodeMCU, D1 Mini, Wemos D1*