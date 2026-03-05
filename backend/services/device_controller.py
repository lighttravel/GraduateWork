"""
Device Controller Interface.
Hardware abstraction layer for aromatherapy device control.

This interface decouples the business logic from hardware implementation,
enabling easy swapping between:
- Virtual device (visualization-only, current implementation)
- Physical device (ESP32 via MQTT/HTTP/Serial, future implementation)
"""
from abc import ABC, abstractmethod
from typing import Dict, Any, Optional
import logging

logger = logging.getLogger(__name__)


class DeviceController(ABC):
    """
    Abstract base class for aromatherapy device controllers.
    Defines the contract for all device implementations.
    """

    @abstractmethod
    async def execute_command(self, control_params: Dict[str, Any]) -> bool:
        """
        Execute aromatherapy control command on the device.

        Args:
            control_params: Dictionary containing:
                - scent_type: str
                - intensity: int (1-10)
                - duration_minutes: int (5-120)
                - release_rhythm: str
                - mixing_ratios: Optional[Dict[str, float]]

        Returns:
            True if command executed successfully, False otherwise

        Raises:
            NotImplementedError: Subclass must implement this method
        """
        raise NotImplementedError

    @abstractmethod
    async def get_device_status(self) -> Dict[str, Any]:
        """
        Get current device status.

        Returns:
            Dictionary containing device state:
                - is_active: bool
                - current_scent: str
                - current_intensity: int
                - remaining_minutes: int
                - error: Optional[str]
        """
        raise NotImplementedError

    @abstractmethod
    async def stop(self) -> bool:
        """
        Stop the device (emergency stop or user-requested).

        Returns:
            True if stopped successfully
        """
        raise NotImplementedError


class VirtualDeviceController(DeviceController):
    """
    Virtual device controller for visualization-only mode.
    Stores state in-memory without controlling physical hardware.

    Future: Replace with ESP32Controller, MQTTDeviceController, etc.
    """

    def __init__(self):
        """Initialize virtual device with default state."""
        self.state = {
            "is_active": False,
            "current_scent": None,
            "current_intensity": 0,
            "remaining_minutes": 0,
            "control_params": {},
            "error": None,
        }
        logger.info("Virtual Device Controller initialized (visualization mode)")

    async def execute_command(self, control_params: Dict[str, Any]) -> bool:
        """
        Execute command by updating internal state (virtual mode).

        In physical mode, this would:
        1. Validate parameters
        2. Send command to ESP32 via MQTT/HTTP/Serial
        3. Wait for acknowledgment
        4. Update state based on device response

        Args:
            control_params: Control parameters from LLM

        Returns:
            True (always succeeds in virtual mode)
        """
        try:
            logger.info(f"Executing virtual command: {control_params}")

            # Update virtual device state
            self.state["is_active"] = True
            self.state["current_scent"] = control_params.get("scent_type")
            self.state["current_intensity"] = control_params.get("intensity", 0)
            self.state["remaining_minutes"] = control_params.get("duration_minutes", 0)
            self.state["control_params"] = control_params
            self.state["error"] = None

            logger.info(
                f"Virtual device state updated: scent={self.state['current_scent']}, "
                f"intensity={self.state['current_intensity']}, "
                f"duration={self.state['remaining_minutes']}min"
            )

            # TODO: When physical device is connected:
            # await self._send_to_esp32(control_params)
            # response = await self._wait_for_ack()
            # if response.status != "ok":
            #     raise DeviceError(response.error)

            return True

        except Exception as e:
            logger.error(f"Virtual device command execution error: {e}")
            self.state["error"] = str(e)
            return False

    async def get_device_status(self) -> Dict[str, Any]:
        """
        Get current virtual device status.

        In physical mode, this would query the actual device.

        Returns:
            Current device state
        """
        # TODO: When physical device is connected:
        # status = await self._query_esp32_status()
        # return status

        return {
            "is_active": self.state["is_active"],
            "current_scent": self.state["current_scent"],
            "current_intensity": self.state["current_intensity"],
            "remaining_minutes": self.state["remaining_minutes"],
            "control_params": self.state["control_params"],
            "error": self.state["error"],
        }

    async def stop(self) -> bool:
        """
        Stop the virtual device.

        In physical mode, this would send stop command to hardware.

        Returns:
            True (always succeeds in virtual mode)
        """
        logger.info("Stopping virtual device")
        self.state["is_active"] = False
        self.state["remaining_minutes"] = 0

        # TODO: When physical device is connected:
        # await self._send_stop_command_to_esp32()

        return True


# Global device controller instance (Virtual mode)
# Future: Replace with ESP32Controller(mqtt_client, device_id)
device_controller = VirtualDeviceController()
