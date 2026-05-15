"""GPIO Emulator - Mock GPIO for development/testing without RPi hardware.

Re-exports MockGPIOController for convenient importing."""

from app.gpio_interface import MockGPIOController

__all__ = ["MockGPIOController"]
