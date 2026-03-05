"""
Command WebSocket Router.
Provides WebSocket endpoint for real-time command execution and device state synchronization.
Supports multi-client broadcasting for synchronized dashboards.
"""
import asyncio
import json
import logging
from typing import Dict, Set
from fastapi import APIRouter, WebSocket, WebSocketDisconnect, Depends
from fastapi.websockets import WebSocketState
from sqlalchemy.ext.asyncio import AsyncSession

from database import get_db_session
from services.command_executor import command_executor

logger = logging.getLogger(__name__)

router = APIRouter()

# Connected clients registry (for multi-client broadcasting)
# Key: room_id (default: "aromatherapy_device_1")
# Value: Set of WebSocket connections
connected_clients: Dict[str, Set[WebSocket]] = {}
DEFAULT_ROOM = "aromatherapy_device_1"


class ConnectionManager:
    """
    Manages WebSocket connections for multi-client broadcasting.
    Ensures all dashboards see the same device state in real-time.
    """

    def __init__(self):
        self.active_connections: Dict[str, Set[WebSocket]] = {}

    async def connect(self, websocket: WebSocket, room_id: str = DEFAULT_ROOM):
        """Add client to room."""
        await websocket.accept()

        if room_id not in self.active_connections:
            self.active_connections[room_id] = set()

        self.active_connections[room_id].add(websocket)
        logger.info(f"Client connected to room '{room_id}' (total: {len(self.active_connections[room_id])})")

    def disconnect(self, websocket: WebSocket, room_id: str = DEFAULT_ROOM):
        """Remove client from room."""
        if room_id in self.active_connections:
            self.active_connections[room_id].discard(websocket)
            remaining = len(self.active_connections[room_id])
            logger.info(f"Client disconnected from room '{room_id}' (remaining: {remaining})")

            # Clean up empty rooms
            if remaining == 0:
                del self.active_connections[room_id]

    async def broadcast(self, message: dict, room_id: str = DEFAULT_ROOM, exclude: WebSocket = None):
        """
        Broadcast message to all clients in room.

        Args:
            message: Message to broadcast
            room_id: Room identifier
            exclude: Optional WebSocket to exclude from broadcast
        """
        if room_id not in self.active_connections:
            return

        dead_connections = set()

        for connection in self.active_connections[room_id]:
            if connection == exclude:
                continue

            try:
                if connection.client_state == WebSocketState.CONNECTED:
                    await connection.send_json(message)
            except Exception as e:
                logger.error(f"Error broadcasting to client: {e}")
                dead_connections.add(connection)

        # Remove dead connections
        for dead in dead_connections:
            self.disconnect(dead, room_id)


manager = ConnectionManager()


@router.websocket("/ws/commands")
async def command_websocket(
    websocket: WebSocket,
    room_id: str = DEFAULT_ROOM,
):
    """
    WebSocket endpoint for real-time command execution and device state sync.

    Multi-Client Synchronization:
        - All clients join the same room (default: "aromatherapy_device_1")
        - Command execution events are broadcast to all clients
        - Device state changes are visible across all dashboards

    Client → Server Messages:
        1. Execute Command:
           {
               "type": "execute_command",
               "user_input": "make the room smell fresh"
           }

        2. Get Device Status:
           {
               "type": "get_status"
           }

        3. Stop Device:
           {
               "type": "stop_device"
           }

    Server → Client Events:
        - llm_processing: {"text": "..."}
        - command_generated: {"control_json": {...}, "response_text": "..."}
        - command_saved: {"command_id": "..."}
        - device_executing: {"command_id": "..."}
        - device_executed: {"command_id": "..."}
        - tts_generating: {"text": "..."}
        - tts_ready: {"audio_base64": "...", "size_bytes": 123}
        - execution_complete: {"command_id": "...", "status": "executed"}
        - execution_error: {"error": "..."}
        - device_status: {device state}
    """
    await manager.connect(websocket, room_id)

    # Send current device status on connection
    try:
        status = await command_executor.get_latest_device_status()
        await websocket.send_json({"type": "device_status", "data": status})
    except Exception as e:
        logger.error(f"Error sending initial device status: {e}")

    try:
        while True:
            # Receive message from client
            data = await websocket.receive_text()

            try:
                message = json.loads(data)
                msg_type = message.get("type")

                logger.info(f"Received message type: {msg_type} from room '{room_id}'")

                # ========== Execute Command ==========
                if msg_type == "execute_command":
                    user_input = message.get("user_input")

                    if not user_input:
                        await websocket.send_json({
                            "type": "error",
                            "message": "user_input is required"
                        })
                        continue

                    # Event callback for broadcasting to all clients
                    async def on_event(event_type: str, event_data: dict):
                        event_message = {"type": event_type, "data": event_data}
                        await manager.broadcast(event_message, room_id)

                    # Execute command with database session
                    try:
                        # Get database session using dependency injection pattern
                        # Note: In WebSocket context, we need to manage session manually
                        from database import async_session_maker

                        async with async_session_maker() as session:
                            result = await command_executor.execute_command(
                                user_input_text=user_input,
                                session=session,
                                on_event=on_event,
                            )

                        # Send final result to client
                        await manager.broadcast(
                            {
                                "type": "command_result",
                                "data": result,
                            },
                            room_id,
                        )

                    except Exception as e:
                        logger.error(f"Command execution error: {e}")
                        await manager.broadcast(
                            {
                                "type": "execution_error",
                                "data": {"error": str(e)},
                            },
                            room_id,
                        )

                # ========== Get Device Status ==========
                elif msg_type == "get_status":
                    status = await command_executor.get_latest_device_status()
                    await websocket.send_json({
                        "type": "device_status",
                        "data": status,
                    })

                # ========== Stop Device ==========
                elif msg_type == "stop_device":
                    success = await command_executor.stop_device()

                    # Broadcast stop event to all clients
                    await manager.broadcast(
                        {
                            "type": "device_stopped",
                            "data": {"success": success},
                        },
                        room_id,
                    )

                    # Send updated status
                    status = await command_executor.get_latest_device_status()
                    await manager.broadcast(
                        {
                            "type": "device_status",
                            "data": status,
                        },
                        room_id,
                    )

                # ========== Unknown Message Type ==========
                else:
                    await websocket.send_json({
                        "type": "error",
                        "message": f"Unknown message type: {msg_type}"
                    })

            except json.JSONDecodeError as e:
                logger.error(f"Invalid JSON from client: {e}")
                await websocket.send_json({
                    "type": "error",
                    "message": "Invalid JSON format"
                })

    except WebSocketDisconnect:
        manager.disconnect(websocket, room_id)
        logger.info(f"Client disconnected from room '{room_id}'")

    except Exception as e:
        logger.error(f"WebSocket error: {e}")
        manager.disconnect(websocket, room_id)

    finally:
        if websocket.client_state == WebSocketState.CONNECTED:
            await websocket.close()
