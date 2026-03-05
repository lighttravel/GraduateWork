import { act, renderHook, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useAudioCapture } from '@/hooks/useAudioCapture';

type AudioProcessHandler = ((event: AudioProcessingEvent) => void) | null;

class MockScriptProcessorNode {
  public onaudioprocess: AudioProcessHandler = null;
  connect = vi.fn();
  disconnect = vi.fn();
}

class MockMediaStreamSourceNode {
  connect = vi.fn();
  disconnect = vi.fn();
}

let latestProcessorNode: MockScriptProcessorNode | null = null;

class MockAudioContext {
  sampleRate = 48000;
  destination = {} as AudioNode;
  createMediaStreamSource = vi.fn(() => new MockMediaStreamSourceNode() as unknown as MediaStreamAudioSourceNode);
  createScriptProcessor = vi.fn(() => {
    latestProcessorNode = new MockScriptProcessorNode();
    return latestProcessorNode as unknown as ScriptProcessorNode;
  });
  close = vi.fn(() => Promise.resolve());
}

describe('useAudioCapture', () => {
  const getUserMediaMock = vi.fn();

  beforeEach(() => {
    latestProcessorNode = null;
    getUserMediaMock.mockReset();

    Object.defineProperty(navigator, 'mediaDevices', {
      configurable: true,
      value: {
        getUserMedia: getUserMediaMock,
      },
    });

    vi.stubGlobal('AudioContext', MockAudioContext);
  });

  it('handles microphone permission denial', async () => {
    getUserMediaMock.mockRejectedValue(new DOMException('Denied', 'NotAllowedError'));

    const { result } = renderHook(() => useAudioCapture());
    let started = true;

    await act(async () => {
      started = await result.current.startRecording();
    });

    expect(started).toBe(false);
    expect(result.current.isRecording).toBe(false);
    expect(result.current.hasPermission).toBe(false);
    expect(result.current.error).toContain('Microphone permission denied');
  });

  it('starts and stops recording, producing PCM blob and chunks', async () => {
    const stopTrack = vi.fn();
    const mockStream = {
      getTracks: () => [{ stop: stopTrack }],
    } as unknown as MediaStream;

    getUserMediaMock.mockResolvedValue(mockStream);
    const onPCMChunk = vi.fn();

    const { result } = renderHook(() => useAudioCapture({ onPCMChunk }));
    let started = false;

    await act(async () => {
      started = await result.current.startRecording();
    });

    expect(started).toBe(true);
    expect(result.current.isRecording).toBe(true);
    expect(result.current.hasPermission).toBe(true);
    expect(latestProcessorNode).not.toBeNull();

    act(() => {
      latestProcessorNode?.onaudioprocess?.({
        inputBuffer: {
          getChannelData: () => new Float32Array([0, 0.35, -0.35, 0.75, -0.75]),
        },
      } as unknown as AudioProcessingEvent);
    });

    act(() => {
      result.current.stopRecording();
    });

    await waitFor(() => {
      expect(result.current.isRecording).toBe(false);
      expect(result.current.audioChunks.length).toBeGreaterThan(0);
      expect(result.current.audioBlob).not.toBeNull();
    });

    expect(onPCMChunk).toHaveBeenCalledTimes(1);
    expect(stopTrack).toHaveBeenCalled();
  });
});
