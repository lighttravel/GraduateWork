import pytest

from services.device_controller import VirtualDeviceController


@pytest.mark.asyncio
async def test_execute_command_updates_virtual_state():
    controller = VirtualDeviceController()

    command = {
        "scent_type": "lavender",
        "intensity": 6,
        "duration_minutes": 25,
        "release_rhythm": "gradual",
    }

    success = await controller.execute_command(command)
    status = await controller.get_device_status()

    assert success is True
    assert status["is_active"] is True
    assert status["current_scent"] == "lavender"
    assert status["current_intensity"] == 6
    assert status["remaining_minutes"] == 25
    assert status["control_params"] == command


@pytest.mark.asyncio
async def test_get_device_status_defaults_after_init():
    controller = VirtualDeviceController()
    status = await controller.get_device_status()

    assert status["is_active"] is False
    assert status["current_scent"] is None
    assert status["current_intensity"] == 0
    assert status["remaining_minutes"] == 0
    assert status["control_params"] == {}
    assert status["error"] is None


@pytest.mark.asyncio
async def test_stop_turns_off_device_and_clears_remaining_time():
    controller = VirtualDeviceController()

    await controller.execute_command(
        {
            "scent_type": "lemon",
            "intensity": 8,
            "duration_minutes": 40,
            "release_rhythm": "pulse",
        }
    )
    stopped = await controller.stop()
    status = await controller.get_device_status()

    assert stopped is True
    assert status["is_active"] is False
    assert status["remaining_minutes"] == 0
