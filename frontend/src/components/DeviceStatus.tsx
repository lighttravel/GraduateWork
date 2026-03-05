import { useMemo } from 'react';
import { useCommandStore } from '@/store/commandStore';

function formatRemainingMinutes(minutes: number): string {
  const totalSeconds = Math.max(0, Math.round(minutes * 60));
  const mm = String(Math.floor(totalSeconds / 60)).padStart(2, '0');
  const ss = String(totalSeconds % 60).padStart(2, '0');
  return `${mm}:${ss}`;
}

const SCENT_COLORS: Record<string, string> = {
  lemon: '#FFD700',
  lavender: '#E6E6FA',
  woody: '#8B4513',
  floral: '#FFB6C1',
  mixed: '#7FC8A9',
};

export default function DeviceStatus() {
  const deviceStatus = useCommandStore((state) => state.deviceStatus);

  const intensityPercent = useMemo(() => {
    const intensity = deviceStatus?.current_intensity ?? 0;
    return Math.max(0, Math.min(100, (intensity / 10) * 100));
  }, [deviceStatus?.current_intensity]);

  const scent = deviceStatus?.current_scent ?? 'none';
  const scentColor = SCENT_COLORS[scent] ?? '#94A3B8';

  return (
    <div className="device-status">
      <div className="device-status-header">
        <span className={`device-status-badge ${deviceStatus?.is_active ? 'is-active' : ''}`}>
          {deviceStatus?.is_active ? 'Active' : 'Idle'}
        </span>
      </div>

      <div className="device-status-grid">
        <article className="device-stat">
          <h3>Current Scent</h3>
          <p className="device-scent">
            <span className="device-scent-dot" style={{ backgroundColor: scentColor }} aria-hidden="true" />
            <span>{scent}</span>
          </p>
        </article>

        <article className="device-stat">
          <h3>Intensity</h3>
          <p>{deviceStatus?.current_intensity ?? 0} / 10</p>
          <div className="meter">
            <span style={{ width: `${intensityPercent}%` }} />
          </div>
        </article>

        <article className="device-stat">
          <h3>Remaining Time</h3>
          <p>{formatRemainingMinutes(deviceStatus?.remaining_minutes ?? 0)}</p>
        </article>
      </div>

      {deviceStatus?.error ? <p className="error-text">{deviceStatus.error}</p> : null}
    </div>
  );
}
