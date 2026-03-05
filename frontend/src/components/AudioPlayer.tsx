import { useEffect } from 'react';
import { useAudioPlayback } from '@/hooks/useAudioPlayback';

interface AudioPlayerProps {
  audioBase64: string | null;
}

function formatTime(seconds: number): string {
  if (!Number.isFinite(seconds) || seconds < 0) {
    return '00:00';
  }
  const whole = Math.floor(seconds);
  const mm = String(Math.floor(whole / 60)).padStart(2, '0');
  const ss = String(whole % 60).padStart(2, '0');
  return `${mm}:${ss}`;
}

export default function AudioPlayer({ audioBase64 }: AudioPlayerProps) {
  const {
    play,
    pause,
    stop,
    setVolume,
    markUserInteraction,
    isPlaying,
    currentTime,
    duration,
    volume,
    hasUserInteracted,
    error,
  } = useAudioPlayback(0.85);

  useEffect(() => {
    if (!audioBase64 || !hasUserInteracted) {
      return;
    }
    void play(audioBase64);
  }, [audioBase64, hasUserInteracted, play]);

  const hasAudio = Boolean(audioBase64);
  const progressPercent = duration > 0 ? Math.max(0, Math.min(100, (currentTime / duration) * 100)) : 0;

  return (
    <div className="audio-player">
      <header className="panel-header">
        <h3>TTS Player</h3>
        <span className={`badge ${hasAudio ? 'badge-ok' : 'badge-neutral'}`}>
          {hasAudio ? 'Audio Ready' : 'Waiting'}
        </span>
      </header>

      <div className="audio-controls">
        {!hasUserInteracted ? (
          <button
            type="button"
            className="btn-secondary"
            onClick={() => {
              markUserInteraction();
            }}
          >
            Enable Audio
          </button>
        ) : (
          <>
            <button
              type="button"
              className="btn-secondary"
              disabled={!hasAudio}
              onClick={() => {
                void play(audioBase64 ?? undefined);
              }}
            >
              Play
            </button>
            <button type="button" className="btn-secondary" disabled={!hasAudio || !isPlaying} onClick={pause}>
              Pause
            </button>
            <button type="button" className="btn-secondary" disabled={!hasAudio} onClick={stop}>
              Stop
            </button>
          </>
        )}
      </div>

      <div className="audio-progress-track">
        <span style={{ width: `${progressPercent}%` }} />
      </div>

      <div className="audio-meta">
        <span>{formatTime(currentTime)}</span>
        <span>{formatTime(duration)}</span>
      </div>

      <label className="audio-volume">
        <span>Volume</span>
        <input
          type="range"
          min={0}
          max={100}
          value={Math.round(volume * 100)}
          onChange={(event) => {
            const numeric = Number(event.target.value);
            setVolume(Number.isFinite(numeric) ? numeric / 100 : 0.85);
          }}
        />
      </label>

      {error ? <p className="voice-error-text">{error}</p> : null}
      {!hasAudio ? <p className="muted">A new TTS clip will be loaded from `tts_ready` events.</p> : null}
    </div>
  );
}
