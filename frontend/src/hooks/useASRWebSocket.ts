import { useCallback, useEffect, useRef, useState } from 'react';
import type { ASREvent } from '@/types/aromatherapy';
import { WebSocketManager } from '@/services/apiClient';

interface TranscriptionState {
  partial: string;
  final: string;
}

interface UseASRWebSocketResult {
  connect: () => Promise<boolean>;
  disconnect: () => void;
  sendAudio: (audio: ArrayBuffer | ArrayBufferView | Blob) => void;
  endStream: () => void;
  resetTranscription: () => void;
  transcription: TranscriptionState;
  isConnected: boolean;
  error: string | null;
}

const CONNECT_TIMEOUT_MS = 3000;

export function useASRWebSocket(): UseASRWebSocketResult {
  const managerRef = useRef<WebSocketManager | null>(null);

  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [transcription, setTranscription] = useState<TranscriptionState>({
    partial: '',
    final: '',
  });

  const createManagerIfNeeded = useCallback((): WebSocketManager => {
    if (managerRef.current) {
      return managerRef.current;
    }

    const manager = new WebSocketManager('/ws/asr', {
      autoReconnect: false,
    });

    manager.onOpen = () => {
      setIsConnected(true);
      setError(null);
    };

    manager.onClose = () => {
      setIsConnected(false);
    };

    manager.onError = () => {
      setError('ASR service connection failed.');
    };

    manager.onMessage = (event: MessageEvent) => {
      try {
        const parsed = JSON.parse(String(event.data)) as ASREvent;

        if (parsed.type === 'partial') {
          setTranscription((prev) => ({
            ...prev,
            partial: parsed.text ?? '',
          }));
          return;
        }

        if (parsed.type === 'final') {
          const finalText = parsed.text ?? '';
          setTranscription({
            partial: '',
            final: finalText,
          });
          return;
        }

        if (parsed.type === 'error') {
          setError(parsed.message ?? 'ASR transcription error.');
        }
      } catch {
        setError('Received invalid ASR payload from server.');
      }
    };

    managerRef.current = manager;
    return manager;
  }, []);

  const connect = useCallback(async (): Promise<boolean> => {
    const manager = createManagerIfNeeded();
    setError(null);

    if (manager.isConnected) {
      setIsConnected(true);
      return true;
    }

    manager.connect();

    const start = Date.now();
    while (Date.now() - start < CONNECT_TIMEOUT_MS) {
      if (manager.isConnected) {
        return true;
      }
      await new Promise((resolve) => {
        window.setTimeout(resolve, 80);
      });
    }

    setError('Unable to connect to ASR service.');
    return false;
  }, [createManagerIfNeeded]);

  const disconnect = useCallback(() => {
    if (managerRef.current) {
      managerRef.current.close();
    }
    setIsConnected(false);
  }, []);

  const sendAudio = useCallback((audio: ArrayBuffer | ArrayBufferView | Blob) => {
    if (!managerRef.current) {
      return;
    }
    managerRef.current.send(audio);
  }, []);

  const endStream = useCallback(() => {
    if (!managerRef.current) {
      return;
    }
    managerRef.current.send({ type: 'end' });
  }, []);

  const resetTranscription = useCallback(() => {
    setTranscription({
      partial: '',
      final: '',
    });
    setError(null);
  }, []);

  useEffect(() => {
    return () => {
      if (managerRef.current) {
        managerRef.current.close();
      }
    };
  }, []);

  return {
    connect,
    disconnect,
    sendAudio,
    endStream,
    resetTranscription,
    transcription,
    isConnected,
    error,
  };
}
