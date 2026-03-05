import { useCallback, useEffect, useRef, useState } from 'react';

interface UseAudioPlaybackResult {
  play: (audioBase64?: string) => Promise<void>;
  pause: () => void;
  stop: () => void;
  setVolume: (value: number) => void;
  isPlaying: boolean;
  currentTime: number;
  duration: number;
  volume: number;
  error: string | null;
}

const DEFAULT_MIME_TYPE = 'audio/mpeg';

export function useAudioPlayback(initialVolume = 1): UseAudioPlaybackResult {
  const audioRef = useRef<HTMLAudioElement | null>(null);
  const sourceBase64Ref = useRef<string | null>(null);

  const [isPlaying, setIsPlaying] = useState(false);
  const [currentTime, setCurrentTime] = useState(0);
  const [duration, setDuration] = useState(0);
  const [volume, setVolumeState] = useState(Math.max(0, Math.min(1, initialVolume)));
  const [error, setError] = useState<string | null>(null);

  const ensureAudio = useCallback((): HTMLAudioElement => {
    if (audioRef.current) {
      return audioRef.current;
    }

    const audio = new Audio();
    audio.preload = 'auto';
    audio.volume = volume;

    audio.addEventListener('play', () => {
      setIsPlaying(true);
    });
    audio.addEventListener('pause', () => {
      setIsPlaying(false);
    });
    audio.addEventListener('ended', () => {
      setIsPlaying(false);
      setCurrentTime(audio.duration || 0);
    });
    audio.addEventListener('timeupdate', () => {
      setCurrentTime(audio.currentTime);
    });
    audio.addEventListener('loadedmetadata', () => {
      setDuration(audio.duration || 0);
    });
    audio.addEventListener('error', () => {
      setError('Audio playback failed.');
    });

    audioRef.current = audio;
    return audio;
  }, [volume]);

  const setSourceFromBase64 = useCallback((audioBase64: string) => {
    const audio = ensureAudio();
    if (sourceBase64Ref.current === audioBase64) {
      return;
    }

    sourceBase64Ref.current = audioBase64;
    const dataUrl = `data:${DEFAULT_MIME_TYPE};base64,${audioBase64}`;
    audio.src = dataUrl;
    audio.currentTime = 0;
    setCurrentTime(0);
    setDuration(0);
  }, [ensureAudio]);

  const play = useCallback(async (audioBase64?: string) => {
    const audio = ensureAudio();

    if (audioBase64) {
      setSourceFromBase64(audioBase64);
    }

    if (!audio.src) {
      setError('No audio source available.');
      return;
    }

    try {
      setError(null);
      await audio.play();
    } catch (playError) {
      setIsPlaying(false);
      if (playError instanceof DOMException && playError.name === 'NotAllowedError') {
        setError('Autoplay was blocked by browser policy. Tap Play to start audio.');
      } else {
        setError('Unable to start audio playback.');
      }
    }
  }, [ensureAudio, setSourceFromBase64]);

  const pause = useCallback(() => {
    if (audioRef.current) {
      audioRef.current.pause();
    }
  }, []);

  const stop = useCallback(() => {
    if (audioRef.current) {
      audioRef.current.pause();
      audioRef.current.currentTime = 0;
      setCurrentTime(0);
      setIsPlaying(false);
    }
  }, []);

  const setVolume = useCallback((value: number) => {
    const next = Math.max(0, Math.min(1, value));
    setVolumeState(next);
    if (audioRef.current) {
      audioRef.current.volume = next;
    }
  }, []);

  useEffect(() => {
    return () => {
      if (audioRef.current) {
        audioRef.current.pause();
        audioRef.current.src = '';
        audioRef.current.load();
      }
      audioRef.current = null;
      sourceBase64Ref.current = null;
    };
  }, []);

  return {
    play,
    pause,
    stop,
    setVolume,
    isPlaying,
    currentTime,
    duration,
    volume,
    error,
  };
}
