import { useCallback, useEffect, useRef, useState } from 'react';

interface UseAudioCaptureOptions {
  onPCMChunk?: (chunk: Int16Array) => void;
  targetSampleRate?: number;
  bufferSize?: number;
}

interface UseAudioCaptureResult {
  startRecording: () => Promise<void>;
  stopRecording: () => void;
  audioBlob: Blob | null;
  audioChunks: Blob[];
  isRecording: boolean;
  error: string | null;
  hasPermission: boolean;
}

function mapMediaError(error: unknown): string {
  if (error instanceof DOMException) {
    if (error.name === 'NotAllowedError') {
      return 'Microphone permission denied. Please allow microphone access.';
    }
    if (error.name === 'NotFoundError') {
      return 'No microphone device found.';
    }
    if (error.name === 'NotReadableError') {
      return 'Microphone is busy or cannot be read.';
    }
  }

  return 'Failed to capture audio from microphone.';
}

function downsampleBuffer(
  input: Float32Array,
  inputSampleRate: number,
  outputSampleRate: number
): Float32Array {
  if (outputSampleRate >= inputSampleRate) {
    return input;
  }

  const sampleRateRatio = inputSampleRate / outputSampleRate;
  const outputLength = Math.round(input.length / sampleRateRatio);
  const output = new Float32Array(outputLength);

  let outputIndex = 0;
  let inputIndex = 0;

  while (outputIndex < outputLength) {
    const nextInputIndex = Math.round((outputIndex + 1) * sampleRateRatio);
    let accumulator = 0;
    let count = 0;

    for (let i = inputIndex; i < nextInputIndex && i < input.length; i += 1) {
      accumulator += input[i];
      count += 1;
    }

    output[outputIndex] = count > 0 ? accumulator / count : 0;
    outputIndex += 1;
    inputIndex = nextInputIndex;
  }

  return output;
}

function floatToInt16PCM(input: Float32Array): Int16Array {
  const output = new Int16Array(input.length);

  for (let i = 0; i < input.length; i += 1) {
    const clamped = Math.max(-1, Math.min(1, input[i]));
    output[i] = clamped < 0 ? clamped * 0x8000 : clamped * 0x7fff;
  }

  return output;
}

function mergeInt16Chunks(chunks: Int16Array[]): Int16Array {
  const totalLength = chunks.reduce((sum, chunk) => sum + chunk.length, 0);
  const merged = new Int16Array(totalLength);

  let offset = 0;
  for (const chunk of chunks) {
    merged.set(chunk, offset);
    offset += chunk.length;
  }

  return merged;
}

function int16ToArrayBuffer(chunk: Int16Array): ArrayBuffer {
  const buffer = new ArrayBuffer(chunk.byteLength);
  const view = new Uint8Array(buffer);
  view.set(new Uint8Array(chunk.buffer, chunk.byteOffset, chunk.byteLength));
  return buffer;
}

export function useAudioCapture(options: UseAudioCaptureOptions = {}): UseAudioCaptureResult {
  const { onPCMChunk, targetSampleRate = 16000, bufferSize = 4096 } = options;

  const [audioBlob, setAudioBlob] = useState<Blob | null>(null);
  const [audioChunks, setAudioChunks] = useState<Blob[]>([]);
  const [isRecording, setIsRecording] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [hasPermission, setHasPermission] = useState(false);

  const streamRef = useRef<MediaStream | null>(null);
  const audioContextRef = useRef<AudioContext | null>(null);
  const sourceNodeRef = useRef<MediaStreamAudioSourceNode | null>(null);
  const processorNodeRef = useRef<ScriptProcessorNode | null>(null);
  const pcmChunksRef = useRef<Int16Array[]>([]);
  const chunkBlobsRef = useRef<Blob[]>([]);

  const cleanupResources = useCallback(() => {
    if (processorNodeRef.current) {
      processorNodeRef.current.onaudioprocess = null;
      processorNodeRef.current.disconnect();
      processorNodeRef.current = null;
    }

    if (sourceNodeRef.current) {
      sourceNodeRef.current.disconnect();
      sourceNodeRef.current = null;
    }

    if (streamRef.current) {
      streamRef.current.getTracks().forEach((track) => track.stop());
      streamRef.current = null;
    }

    if (audioContextRef.current) {
      void audioContextRef.current.close();
      audioContextRef.current = null;
    }
  }, []);

  const startRecording = useCallback(async () => {
    if (isRecording) {
      return;
    }

    setError(null);
    setAudioBlob(null);
    setAudioChunks([]);
    pcmChunksRef.current = [];
    chunkBlobsRef.current = [];

    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          channelCount: 1,
          noiseSuppression: true,
          echoCancellation: true,
          autoGainControl: true,
        },
      });

      setHasPermission(true);
      streamRef.current = stream;

      const audioContextFactory =
        window.AudioContext ??
        (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;

      if (!audioContextFactory) {
        throw new Error('AudioContext is not supported in this browser.');
      }

      const audioContext = new audioContextFactory();
      audioContextRef.current = audioContext;

      const sourceNode = audioContext.createMediaStreamSource(stream);
      sourceNodeRef.current = sourceNode;

      const processorNode = audioContext.createScriptProcessor(bufferSize, 1, 1);
      processorNodeRef.current = processorNode;

      processorNode.onaudioprocess = (event: AudioProcessingEvent) => {
        const inputData = event.inputBuffer.getChannelData(0);
        const downsampled = downsampleBuffer(inputData, audioContext.sampleRate, targetSampleRate);
        const pcmChunk = floatToInt16PCM(downsampled);

        if (pcmChunk.length === 0) {
          return;
        }

        pcmChunksRef.current.push(pcmChunk);
        chunkBlobsRef.current.push(new Blob([int16ToArrayBuffer(pcmChunk)], { type: 'application/octet-stream' }));

        if (onPCMChunk) {
          onPCMChunk(pcmChunk);
        }
      };

      sourceNode.connect(processorNode);
      processorNode.connect(audioContext.destination);

      setIsRecording(true);
    } catch (captureError) {
      setHasPermission(false);
      setError(mapMediaError(captureError));
      cleanupResources();
      setIsRecording(false);
    }
  }, [bufferSize, cleanupResources, isRecording, onPCMChunk, targetSampleRate]);

  const stopRecording = useCallback(() => {
    if (!isRecording) {
      cleanupResources();
      return;
    }

    cleanupResources();
    setIsRecording(false);
    setAudioChunks([...chunkBlobsRef.current]);

    if (pcmChunksRef.current.length > 0) {
      const merged = mergeInt16Chunks(pcmChunksRef.current);
      setAudioBlob(new Blob([int16ToArrayBuffer(merged)], { type: 'audio/pcm' }));
    } else {
      setAudioBlob(null);
    }
  }, [cleanupResources, isRecording]);

  useEffect(() => {
    return () => {
      cleanupResources();
    };
  }, [cleanupResources]);

  return {
    startRecording,
    stopRecording,
    audioBlob,
    audioChunks,
    isRecording,
    error,
    hasPermission,
  };
}
