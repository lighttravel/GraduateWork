import { useMemo } from 'react';
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

export default function CommandPanel() {
  const currentCommand = useCommandStore((state) => state.currentCommand);
  const recentEvents = useCommandStore((state) => state.recentEvents);
  const isExecuting = useCommandStore((state) => state.isExecuting);
  const currentStep = useCommandStore((state) => state.currentStep);
  const executionError = useCommandStore((state) => state.executionError);

  const controlJsonPreview = useMemo(() => {
    const data = currentCommand?.controlJson ?? {};
    return JSON.stringify(data, null, 2);
  }, [currentCommand?.controlJson]);

  return (
    <div className="panel-content">
      <header className="panel-header">
        <h2>Command Pipeline</h2>
        <span className={`badge ${isExecuting ? 'badge-warn' : 'badge-neutral'}`}>
          {isExecuting ? 'Running' : 'Standby'}
        </span>
      </header>

      <div className="pipeline-list">
        {PIPELINE_STEPS.map((step) => (
          <div key={step} className={`pipeline-item ${currentStep === step ? 'pipeline-item-current' : ''}`}>
            <span>{step}</span>
          </div>
        ))}
      </div>

      {executionError ? <p className="error-text">{executionError}</p> : null}

      <div className="code-block">
        <h3>Latest Input</h3>
        <pre>{currentCommand?.userInput || 'No command submitted yet.'}</pre>
      </div>

      <div className="code-block">
        <h3>LLM Response</h3>
        <pre>{currentCommand?.responseText || 'No response yet.'}</pre>
      </div>

      <div className="code-block">
        <h3>Control JSON</h3>
        <pre>{controlJsonPreview}</pre>
      </div>

      <div>
        <h3 className="subheading">Recent Events</h3>
        <ul className="event-list">
          {recentEvents.length === 0 ? <li>No events yet.</li> : null}
          {recentEvents
            .slice()
            .reverse()
            .map((event, index) => (
              <li key={`${event.type}-${index}`}>{event.type}</li>
            ))}
        </ul>
      </div>
    </div>
  );
}
