"""GPIO abstraction layer - abstract interface + RPi implementation + mock.

Pin mapping (from ESP8266):
  LIGHT_PWMPIN   = 5  (GPIO5 / D1 on RPi)
  COOLING_PWMPIN = 6  (GPIO6 / D2 on RPi)
  RELAYLIGHT_PIN  = 1  (GPIO1 / D3 on RPi)
  RELAYCO2_PIN    = 2  (GPIO2 / D4 on RPi)
  RELAYMOON_PIN   = 4  (GPIO4 / D5 on RPi)
  COOLING_OFF_PIN = 0  (GPIO0 / D0 on RPi) - water level sensor
  ONE_WIRE_BUS    = 7  (GPIO7 / D6 on RPi) - DS18B20
"""

from abc import ABC, abstractmethod

# Pin definitions (RPi BCM numbering)
LIGHT_PWMPIN = 5
COOLING_PWMPIN = 6
RELAYLIGHT_PIN = 1
RELAYCO2_PIN = 2
RELAYMOON_PIN = 4
COOLING_OFF_PIN = 0

# Device identifiers (same as ESP8266)
DEVICE_PWMLIGHT = 0
DEVICE_COOLING = 1
DEVICE_LIGHT = 2
DEVICE_CO2 = 3
DEVICE_MOON = 4

# Light levels (7 levels, 10-bit PWM)
LIGHT_LEVELS = [0, 50, 150, 250, 512, 860, 1023]


class GPIOController(ABC):
    """Abstract GPIO interface."""

    @abstractmethod
    def set_pwm(self, pin: int, value: int):
        """Set PWM duty cycle (0-1023)."""
        ...

    @abstractmethod
    def set_digital(self, pin: int, state: bool):
        """Set digital output (True=HIGH, False=LOW)."""
        ...

    @abstractmethod
    def get_digital(self, pin: int) -> bool:
        """Read digital input (True=HIGH, False=LOW)."""
        ...

    @abstractmethod
    def cleanup(self):
        """Reset all pins."""
        ...


class RPiGPIOController(GPIOController):
    """Real RPi GPIO implementation using gpiozero (preferred) or RPi.GPIO."""

    def __init__(self):
        self._devices = {}  # Cache all device instances by pin
        self._initialized = False
        self._use_gpiozero = False
        self._use_rpigpio = False
        self._init_library()

    def _init_library(self):
        """Try to initialize gpiozero first, fall back to RPi.GPIO."""
        try:
            from gpiozero import PWMOutputDevice, DigitalOutputDevice, DigitalInputDevice
            self._gpiozero_PWMOutputDevice = PWMOutputDevice
            self._gpiozero_DigitalOutputDevice = DigitalOutputDevice
            self._gpiozero_DigitalInputDevice = DigitalInputDevice
            self._use_gpiozero = True
        except ImportError:
            try:
                import RPi.GPIO as GPIO
                self._GPIO = GPIO
                self._GPIO.setmode(GPIO.BCM)
                self._GPIO.setwarnings(False)
                self._use_rpigpio = True
            except ImportError:
                raise RuntimeError("No GPIO library available (install gpiozero or RPi.GPIO)")

    def set_pwm(self, pin: int, value: int):
        if self._use_gpiozero:
            if pin not in self._devices:
                self._devices[pin] = self._gpiozero_PWMOutputDevice(pin, frequency=1000)
            # gpiozero uses 0.0 to 1.0 range
            self._devices[pin].value = value / 1023.0
        else:
            self._GPIO.setup(pin, self._GPIO.OUT)
            pwm = self._GPIO.PWM(pin, 1000)
            pwm.start(value / 1023.0 * 100)

    def set_digital(self, pin: int, state: bool):
        if self._use_gpiozero:
            if pin not in self._devices:
                self._devices[pin] = self._gpiozero_DigitalOutputDevice(pin)
            self._devices[pin].value = state
        else:
            self._GPIO.setup(pin, self._GPIO.OUT)
            self._GPIO.output(pin, self._GPIO.HIGH if state else self._GPIO.LOW)

    def get_digital(self, pin: int) -> bool:
        if self._use_gpiozero:
            # Cache input devices to avoid creating new instances on every call
            if pin not in self._devices:
                self._devices[pin] = self._gpiozero_DigitalInputDevice(pin)
            return bool(self._devices[pin].value)
        else:
            self._GPIO.setup(pin, self._GPIO.IN)
            return bool(self._GPIO.input(pin))

    def cleanup(self):
        if self._use_gpiozero:
            for dev in self._devices.values():
                dev.close()
        else:
            self._GPIO.cleanup()
        self._devices.clear()


class MockGPIOController(GPIOController):
    """Mock GPIO for development/testing on non-RPi systems."""

    def __init__(self):
        self._pwm_values: dict[int, int] = {}
        self._digital_values: dict[int, bool] = {}
        self._log: list[str] = []

    def set_pwm(self, pin: int, value: int):
        value = max(0, min(1023, value))
        self._pwm_values[pin] = value
        msg = f"[MOCK] set_pwm(pin={pin}, value={value})"
        self._log.append(msg)
        print(msg)

    def set_digital(self, pin: int, state: bool):
        self._digital_values[pin] = state
        msg = f"[MOCK] set_digital(pin={pin}, state={'HIGH' if state else 'LOW'})"
        self._log.append(msg)
        print(msg)

    def get_digital(self, pin: int) -> bool:
        value = self._digital_values.get(pin, False)
        msg = f"[MOCK] get_digital(pin={pin}) -> {'HIGH' if value else 'LOW'}"
        self._log.append(msg)
        print(msg)
        return value

    def cleanup(self):
        self._pwm_values.clear()
        self._digital_values.clear()
        print("[MOCK] cleanup()")

    def get_pwm(self, pin: int) -> int:
        """Get current PWM value for a pin (mock-specific helper)."""
        return self._pwm_values.get(pin, 0)

    def get_log(self) -> list[str]:
        """Return log of all GPIO operations (mock-specific helper)."""
        return self._log


# Global singleton
_gpio: GPIOController | None = None


def get_gpio() -> GPIOController:
    """Factory: returns MockGPIOController if not on RPi, else RPiGPIOController."""
    global _gpio
    if _gpio is None:
        try:
            import gpiozero
            _gpio = RPiGPIOController()
        except (ImportError, RuntimeError):
            _gpio = MockGPIOController()
            print("[GPIO] RPi GPIO not available - using MockGPIOController")
    return _gpio
