import { useCallback, useEffect, useRef, useState } from 'react';
import { useAudioCapture } from '@/hooks/useAudioCapture';
import { useASRWebSocket } from '@/hooks/useASRWebSocket';

interface VoiceCaptureProps {
  onFinalTranscription?: (text: string) => void;
}

export default function VoiceCapture({ onFinalTranscription }: VoiceCaptureProps) {
  const disconnectTimerRef = useRef<number | null>(null);
  const [isRecognizing, setIsRecognizing] = useState(false);
  const [interactionError, setInteractionError] = useState<string | null>(null);

  const {
    connect,
    disconnect,
    sendAudio,
    endStream,
    resetTranscription,
    transcription,
    error: asrError,
  } = useASRWebSocket();

  const {
    startRecording,
    stopRecording,
    isRecording,
    error: audioError,
    hasPermission,
  } = useAudioCapture({
    onPCMChunk: (chunk) => {
      sendAudio(chunk);
    },
  });

  const clearDisconnectTimer = useCallback(() => {
    if (disconnectTimerRef.current !== null) {
      window.clearTimeout(disconnectTimerRef.current);
      disconnectTimerRef.current = null;
    }
  }, []);

  const handleStartRecording = useCallback(async () => {
    if (isRecording) {
      return;
    }

    setInteractionError(null);
    setIsRecognizing(false);
    resetTranscription();
    clearDisconnectTimer();

    const connected = await connect();
    if (!connected) {
      setInteractionError('ASR service is unavailable.');
      return;
    }

    await startRecording();
  }, [clearDisconnectTimer, connect, isRecording, resetTranscription, startRecording]);

  const handleStopRecording = useCallback(() => {
    if (!isRecording) {
      return;
    }

    stopRecording();
    endStream();
    setIsRecognizing(true);

    clearDisconnectTimer();
    disconnectTimerRef.current = window.setTimeout(() => {
      disconnect();
      setIsRecognizing(false);
    }, 2200);
  }, [clearDisconnectTimer, disconnect, endStream, isRecording, stopRecording]);

  useEffect(() => {
    if (!transcription.final) {
      return;
    }

    if (onFinalTranscription) {
      onFinalTranscription(transcription.final);
    }

    clearDisconnectTimer();
    disconnectTimerRef.current = window.setTimeout(() => {
      disconnect();
      setIsRecognizing(false);
    }, 300);
  }, [clearDisconnectTimer, disconnect, onFinalTranscription, transcription.final]);

  useEffect(() => {
    return () => {
      clearDisconnectTimer();
      disconnect();
    };
  }, [clearDisconnectTimer, disconnect]);

  const displayError = interactionError ?? audioError ?? asrError;
  const statusText = isRecording
    ? 'Recording...'
    : isRecognizing
      ? 'Recognizing speech...'
      : 'Press and hold to talk';

  return (
    <div className="voice-capture">
      <button
        type="button"
        className={`voice-button ${isRecording ? 'voice-button-active' : ''}`}
        onMouseDown={handleStartRecording}
        onMouseUp={handleStopRecording}
        onMouseLeave={handleStopRecording}
        onTouchStart={(event) => {
          event.preventDefault();
          void handleStartRecording();
        }}
        onTouchEnd={(event) => {
          event.preventDefault();
          handleStopRecording();
        }}
        onTouchCancel={(event) => {
          event.preventDefault();
          handleStopRecording();
        }}
        onKeyDown={(event) => {
          if ((event.key === 'Enter' || event.key === ' ') && !isRecording) {
            event.preventDefault();
            void handleStartRecording();
          }
        }}
        onKeyUp={(event) => {
          if (event.key === 'Enter' || event.key === ' ') {
            event.preventDefault();
            handleStopRecording();
          }
        }}
        aria-label="Press and hold to capture voice command"
      >
        <span className="voice-button-ring" aria-hidden="true" />
        <span className="voice-button-icon" aria-hidden="true">
          <svg viewBox="0 0 24 24" role="presentation" focusable="false">
            <path
              d="M12 15a3 3 0 0 0 3-3V7a3 3 0 1 0-6 0v5a3 3 0 0 0 3 3Zm5-3a1 1 0 1 1 2 0 7 7 0 0 1-6 6.93V21h3a1 1 0 1 1 0 2H8a1 1 0 1 1 0-2h3v-2.07A7 7 0 0 1 5 12a1 1 0 1 1 2 0 5 5 0 1 0 10 0Z"
              fill="currentColor"
            />
          </svg>
        </span>
      </button>

      <p className="voice-status-text">{statusText}</p>

      <div className="voice-transcription-box" aria-live="polite">
        {transcription.partial ? (
          <p className="voice-transcription-partial">{transcription.partial}</p>
        ) : null}
        {transcription.final ? <p className="voice-transcription-final">{transcription.final}</p> : null}
        {!transcription.partial && !transcription.final ? (
          <p className="voice-transcription-placeholder">Your transcript will appear here.</p>
        ) : null}
      </div>

      {displayError ? <p className="voice-error-text">{displayError}</p> : null}
      {!hasPermission && !displayError ? (
        <p className="voice-hint-text">Microphone permission is required before first recording.</p>
      ) : null}
    </div>
  );
}
