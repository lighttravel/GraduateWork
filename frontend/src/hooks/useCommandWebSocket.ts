import { useCallback, useEffect, useRef, useState } from 'react';
import { WebSocketManager } from '@/services/apiClient';
import { useCommandStore } from '@/store/commandStore';
import type { ControlJson, DeviceStatus, WSEvent } from '@/types/aromatherapy';

export type CommandWSConnectionStatus = 'connected' | 'reconnecting' | 'disconnected';

interface UseCommandWebSocketOptions {
  autoConnect?: boolean;
  roomId?: string;
}

interface UseCommandWebSocketResult {
  connect: () => void;
  disconnect: () => void;
  isConnected: boolean;
  connectionStatus: CommandWSConnectionStatus;
  sendExecuteCommand: (userInput: string) => void;
  requestDeviceStatus: () => void;
  stopDevice: () => void;
}

function extractString(value: unknown): string | null {
  return typeof value === 'string' ? value : null;
}

function extractControlJson(value: unknown): ControlJson | null {
  if (!value || typeof value !== 'object') {
    return null;
  }
  return value as ControlJson;
}

function extractCommandStatus(value: unknown): 'pending' | 'executed' | 'failed' | null {
  if (value === 'pending' || value === 'executed' || value === 'failed') {
    return value;
  }
  return null;
}

export function useCommandWebSocket(
  options: UseCommandWebSocketOptions = {}
): UseCommandWebSocketResult {
  const { autoConnect = true, roomId } = options;
  const managerRef = useRef<WebSocketManager | null>(null);
  const manualDisconnectRef = useRef(false);

  const [isConnected, setIsConnected] = useState(false);
  const [connectionStatus, setConnectionStatus] = useState<CommandWSConnectionStatus>('disconnected');

  const setDeviceStatus = useCommandStore((state) => state.setDeviceStatus);
  const setCurrentCommand = useCommandStore((state) => state.setCurrentCommand);
  const setIsExecuting = useCommandStore((state) => state.setIsExecuting);
  const setCurrentStep = useCommandStore((state) => state.setCurrentStep);
  const setExecutionError = useCommandStore((state) => state.setExecutionError);
  const addEvent = useCommandStore((state) => state.addEvent);
  const addCommandToHistory = useCommandStore((state) => state.addCommandToHistory);

  const mergeCurrentCommand = useCallback(
    (patch: Partial<{
      commandId: string | null;
      userInput: string;
      controlJson: ControlJson | null;
      responseText: string | null;
      ttsAudioBase64: string | null;
    }>) => {
      const current = useCommandStore.getState().currentCommand;
      setCurrentCommand({
        commandId: patch.commandId ?? current?.commandId ?? null,
        userInput: patch.userInput ?? current?.userInput ?? '',
        controlJson: patch.controlJson ?? current?.controlJson ?? null,
        responseText: patch.responseText ?? current?.responseText ?? null,
        ttsAudioBase64: patch.ttsAudioBase64 ?? current?.ttsAudioBase64 ?? null,
      });
    },
    [setCurrentCommand]
  );

  const createManagerIfNeeded = useCallback((): WebSocketManager => {
    if (managerRef.current) {
      return managerRef.current;
    }

    const endpoint = roomId ? `/ws/commands?room_id=${encodeURIComponent(roomId)}` : '/ws/commands';
    const manager = new WebSocketManager(endpoint);

    manager.onOpen = () => {
      manualDisconnectRef.current = false;
      setIsConnected(true);
      setConnectionStatus('connected');
      setExecutionError(null);
    };

    manager.onClose = () => {
      setIsConnected(false);
      if (manualDisconnectRef.current || !autoConnect) {
        setConnectionStatus('disconnected');
      } else {
        setConnectionStatus('reconnecting');
      }
    };

    manager.onError = () => {
      if (!manualDisconnectRef.current) {
        setConnectionStatus('reconnecting');
      }
      setExecutionError('Command WebSocket connection failed.');
    };

    manager.onMessage = (event: MessageEvent) => {
      try {
        const parsed = JSON.parse(String(event.data)) as WSEvent<Record<string, unknown>>;
        addEvent(parsed);

        const payload = parsed.data ?? {};

        switch (parsed.type) {
          case 'device_status':
            setDeviceStatus(payload as unknown as DeviceStatus);
            break;
          case 'llm_processing':
            setIsExecuting(true);
            setCurrentStep('llm_processing');
            setExecutionError(null);
            mergeCurrentCommand({
              commandId: null,
              userInput: extractString(payload.text) ?? '',
              controlJson: null,
              responseText: null,
              ttsAudioBase64: null,
            });
            break;
          case 'command_generated':
            setCurrentStep('command_generated');
            mergeCurrentCommand({
              controlJson: extractControlJson(payload.control_json),
              responseText: extractString(payload.response_text),
            });
            break;
          case 'command_saved':
            setCurrentStep('command_saved');
            mergeCurrentCommand({
              commandId: extractString(payload.command_id),
            });
            break;
          case 'device_executing':
            setCurrentStep('device_executing');
            break;
          case 'device_executed':
            setCurrentStep('device_executed');
            break;
          case 'device_stopped':
            setCurrentStep('device_stopped');
            setIsExecuting(false);
            break;
          case 'tts_generating':
            setCurrentStep('tts_generating');
            break;
          case 'tts_ready':
            setCurrentStep('tts_ready');
            mergeCurrentCommand({
              ttsAudioBase64: extractString(payload.audio_base64),
            });
            break;
          case 'execution_complete':
            setCurrentStep('execution_complete');
            setIsExecuting(false);
            mergeCurrentCommand({
              commandId: extractString(payload.command_id),
            });
            break;
          case 'command_result':
            {
              const commandId = extractString(payload.command_id);
              const controlJson = extractControlJson(payload.control_json);
              const status = extractCommandStatus(payload.status);
              const responseText = extractString(payload.response_text);
              const executionError = extractString(payload.error);
              const userInput =
                extractString(payload.user_input) ??
                useCommandStore.getState().currentCommand?.userInput ??
                '';

              if (commandId && controlJson && status) {
                const now = new Date().toISOString();
                addCommandToHistory({
                  id: commandId,
                  created_at: now,
                  user_input_text: userInput,
                  llm_response_text: responseText,
                  control_json: controlJson,
                  tts_audio_url: null,
                  status,
                  execution_error: executionError,
                  user_feedback: null,
                  updated_at: now,
                });
              }

              if (executionError) {
                setExecutionError(executionError);
              } else {
                setExecutionError(null);
              }
            }
            mergeCurrentCommand({
              commandId: extractString(payload.command_id),
              controlJson: extractControlJson(payload.control_json),
              responseText: extractString(payload.response_text),
              ttsAudioBase64: extractString(payload.tts_audio_base64),
            });
            setCurrentStep('execution_complete');
            setIsExecuting(false);
            break;
          case 'execution_error':
            setCurrentStep('execution_error');
            setIsExecuting(false);
            setExecutionError(extractString(payload.error) ?? 'Command execution failed.');
            mergeCurrentCommand({
              commandId: extractString(payload.command_id),
            });
            break;
          case 'error':
            setExecutionError(parsed.message ?? 'WebSocket message error.');
            break;
          default:
            break;
        }
      } catch {
        setExecutionError('Received invalid command event payload.');
      }
    };

    managerRef.current = manager;
    return manager;
  }, [
    addCommandToHistory,
    addEvent,
    autoConnect,
    mergeCurrentCommand,
    roomId,
    setCurrentStep,
    setDeviceStatus,
    setExecutionError,
    setIsExecuting,
  ]);

  const connect = useCallback(() => {
    manualDisconnectRef.current = false;
    setConnectionStatus('reconnecting');
    const manager = createManagerIfNeeded();
    manager.connect();
  }, [createManagerIfNeeded]);

  const disconnect = useCallback(() => {
    manualDisconnectRef.current = true;
    if (managerRef.current) {
      managerRef.current.close();
    }
    setIsConnected(false);
    setConnectionStatus('disconnected');
  }, []);

  const sendExecuteCommand = useCallback(
    (userInput: string) => {
      const manager = createManagerIfNeeded();
      manager.send({
        type: 'execute_command',
        user_input: userInput,
      });
    },
    [createManagerIfNeeded]
  );

  const requestDeviceStatus = useCallback(() => {
    const manager = createManagerIfNeeded();
    manager.send({
      type: 'get_status',
    });
  }, [createManagerIfNeeded]);

  const stopDevice = useCallback(() => {
    const manager = createManagerIfNeeded();
    manager.send({
      type: 'stop_device',
    });
  }, [createManagerIfNeeded]);

  useEffect(() => {
    if (autoConnect) {
      connect();
    }

    return () => {
      disconnect();
    };
  }, [autoConnect, connect, disconnect]);

  return {
    connect,
    disconnect,
    isConnected,
    connectionStatus,
    sendExecuteCommand,
    requestDeviceStatus,
    stopDevice,
  };
}
