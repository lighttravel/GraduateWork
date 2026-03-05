import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import VoiceCapture from '@/components/VoiceCapture';
import { useASRWebSocket } from '@/hooks/useASRWebSocket';
import { useAudioCapture } from '@/hooks/useAudioCapture';

vi.mock('@/hooks/useASRWebSocket');
vi.mock('@/hooks/useAudioCapture');

const mockedUseASRWebSocket = vi.mocked(useASRWebSocket);
const mockedUseAudioCapture = vi.mocked(useAudioCapture);

describe('VoiceCapture', () => {
  it('renders partial and final transcription and triggers callback', async () => {
    const onFinalTranscription = vi.fn();

    mockedUseASRWebSocket.mockReturnValue({
      connect: vi.fn().mockResolvedValue(true),
      disconnect: vi.fn(),
      sendAudio: vi.fn(),
      endStream: vi.fn(),
      resetTranscription: vi.fn(),
      transcription: {
        partial: 'partial text',
        final: 'final text',
      },
      isConnected: true,
      error: null,
    });

    mockedUseAudioCapture.mockReturnValue({
      startRecording: vi.fn().mockResolvedValue(true),
      stopRecording: vi.fn(),
      audioBlob: null,
      audioChunks: [],
      isRecording: false,
      error: null,
      hasPermission: true,
    });

    render(<VoiceCapture onFinalTranscription={onFinalTranscription} />);

    expect(screen.getByText('partial text')).toBeInTheDocument();
    expect(screen.getByText('final text')).toBeInTheDocument();

    await waitFor(() => {
      expect(onFinalTranscription).toHaveBeenCalledWith('final text');
    });
  });

  it('starts recording on mouse down when ASR is connected', async () => {
    const connect = vi.fn().mockResolvedValue(true);
    const startRecording = vi.fn().mockResolvedValue(true);

    mockedUseASRWebSocket.mockReturnValue({
      connect,
      disconnect: vi.fn(),
      sendAudio: vi.fn(),
      endStream: vi.fn(),
      resetTranscription: vi.fn(),
      transcription: {
        partial: '',
        final: '',
      },
      isConnected: true,
      error: null,
    });

    mockedUseAudioCapture.mockReturnValue({
      startRecording,
      stopRecording: vi.fn(),
      audioBlob: null,
      audioChunks: [],
      isRecording: false,
      error: null,
      hasPermission: true,
    });

    render(<VoiceCapture />);
    fireEvent.mouseDown(screen.getByRole('button', { name: /press and hold/i }));

    await waitFor(() => {
      expect(connect).toHaveBeenCalled();
      expect(startRecording).toHaveBeenCalled();
    });
  });

  it('stops recording on mouse up when recording is active', () => {
    const stopRecording = vi.fn();
    const endStream = vi.fn();

    mockedUseASRWebSocket.mockReturnValue({
      connect: vi.fn().mockResolvedValue(true),
      disconnect: vi.fn(),
      sendAudio: vi.fn(),
      endStream,
      resetTranscription: vi.fn(),
      transcription: {
        partial: '',
        final: '',
      },
      isConnected: true,
      error: null,
    });

    mockedUseAudioCapture.mockReturnValue({
      startRecording: vi.fn().mockResolvedValue(true),
      stopRecording,
      audioBlob: null,
      audioChunks: [],
      isRecording: true,
      error: null,
      hasPermission: true,
    });

    render(<VoiceCapture />);
    fireEvent.mouseUp(screen.getByRole('button', { name: /press and hold/i }));

    expect(stopRecording).toHaveBeenCalled();
    expect(endStream).toHaveBeenCalled();
  });

  it('disconnects ASR when recording fails to start', async () => {
    const connect = vi.fn().mockResolvedValue(true);
    const disconnect = vi.fn();

    mockedUseASRWebSocket.mockReturnValue({
      connect,
      disconnect,
      sendAudio: vi.fn(),
      endStream: vi.fn(),
      resetTranscription: vi.fn(),
      transcription: {
        partial: '',
        final: '',
      },
      isConnected: true,
      error: null,
    });

    mockedUseAudioCapture.mockReturnValue({
      startRecording: vi.fn().mockResolvedValue(false),
      stopRecording: vi.fn(),
      audioBlob: null,
      audioChunks: [],
      isRecording: false,
      error: 'Microphone permission denied.',
      hasPermission: false,
    });

    render(<VoiceCapture />);
    fireEvent.mouseDown(screen.getByRole('button', { name: /press and hold/i }));

    await waitFor(() => {
      expect(connect).toHaveBeenCalled();
      expect(disconnect).toHaveBeenCalled();
    });

    expect(screen.getByText(/unable to start microphone recording/i)).toBeInTheDocument();
  });
});
