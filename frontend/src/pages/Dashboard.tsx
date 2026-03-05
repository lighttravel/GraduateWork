import { useMemo, useState } from 'react';
import { useCommandStore } from '@/store/commandStore';

const PIPELINE_STEPS = [
  'llm_processing',
  'command_generated',
  'command_saved',
  'device_executing',
  'device_executed',
  'tts_generating',
  'tts_ready',
  'execution_complete',
];

function formatRemainingMinutes(minutes: number): string {
  const totalSeconds = Math.max(0, Math.round(minutes * 60));
  const mm = String(Math.floor(totalSeconds / 60)).padStart(2, '0');
  const ss = String(totalSeconds % 60).padStart(2, '0');
  return `${mm}:${ss}`;
}

export default function Dashboard() {
  const [draftCommand, setDraftCommand] = useState<string>('Make the room feel fresh and light.');

  const deviceStatus = useCommandStore((state) => state.deviceStatus);
  const currentCommand = useCommandStore((state) => state.currentCommand);
  const recentEvents = useCommandStore((state) => state.recentEvents);
  const isExecuting = useCommandStore((state) => state.isExecuting);
  const currentStep = useCommandStore((state) => state.currentStep);
  const executionError = useCommandStore((state) => state.executionError);

  const controlJsonPreview = useMemo(() => {
    const data = currentCommand?.controlJson ?? deviceStatus?.control_params ?? {};
    return JSON.stringify(data, null, 2);
  }, [currentCommand?.controlJson, deviceStatus?.control_params]);

  return (
    <div className="dashboard-grid">
      <section className="panel panel-voice">
        <header className="panel-header">
          <h2>Voice Control</h2>
          <span className="badge badge-warn">Phase 7 Pending</span>
        </header>
        <p className="muted">
          Push-to-talk component is not connected yet. This block is reserved for `VoiceCapture` integration.
        </p>

        <label className="field">
          <span>Command Draft</span>
          <textarea
            value={draftCommand}
            onChange={(event) => setDraftCommand(event.target.value)}
            placeholder="Example: Set lemon scent, intensity 7, duration 20 minutes."
            rows={4}
          />
        </label>
        <button type="button" className="btn-primary" disabled>
          Execute (WebSocket Hook Pending)
        </button>
      </section>

      <section className="panel panel-visual">
        <header className="panel-header">
          <h2>Device Visualization</h2>
          <span className={`badge ${deviceStatus?.is_active ? 'badge-ok' : 'badge-neutral'}`}>
            {deviceStatus?.is_active ? 'Active' : 'Idle'}
          </span>
        </header>

        <div className="stats-grid">
          <article className="stat">
            <h3>Current Scent</h3>
            <p>{deviceStatus?.current_scent ?? 'N/A'}</p>
          </article>
          <article className="stat">
            <h3>Intensity</h3>
            <p>{deviceStatus?.current_intensity ?? 0} / 10</p>
            <div className="meter">
              <span
                style={{
                  width: `${Math.max(0, Math.min(100, ((deviceStatus?.current_intensity ?? 0) / 10) * 100))}%`,
                }}
              />
            </div>
          </article>
          <article className="stat">
            <h3>Remaining Time</h3>
            <p>{formatRemainingMinutes(deviceStatus?.remaining_minutes ?? 0)}</p>
          </article>
        </div>

        <p className="muted">Charts component (`AromatherapyCharts.tsx`) will be mounted here in Phase 8.</p>
      </section>

      <section className="panel panel-command">
        <header className="panel-header">
          <h2>Command Pipeline</h2>
          <span className={`badge ${isExecuting ? 'badge-warn' : 'badge-neutral'}`}>
            {isExecuting ? 'Running' : 'Standby'}
          </span>
        </header>

        <div className="pipeline-list">
          {PIPELINE_STEPS.map((step) => {
            const isCurrent = currentStep === step;
            return (
              <div key={step} className={`pipeline-item ${isCurrent ? 'pipeline-item-current' : ''}`}>
                <span>{step}</span>
              </div>
            );
          })}
        </div>

        {executionError ? <p className="error-text">{executionError}</p> : null}

        <div className="code-block">
          <h3>Control JSON Preview</h3>
          <pre>{controlJsonPreview}</pre>
        </div>

        <div>
          <h3 className="subheading">Recent Events</h3>
          <ul className="event-list">
            {recentEvents.length === 0 ? <li>No events yet.</li> : null}
            {recentEvents.slice().reverse().map((event, index) => (
              <li key={`${event.type}-${index}`}>{event.type}</li>
            ))}
          </ul>
        </div>
      </section>
    </div>
  );
}
