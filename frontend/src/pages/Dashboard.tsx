import { useState } from 'react';
import AromatherapyCharts from '@/components/AromatherapyCharts';
import AudioPlayer from '@/components/AudioPlayer';
import CommandPanel from '@/components/CommandPanel';
import ConnectionStatus from '@/components/ConnectionStatus';
import DeviceStatus from '@/components/DeviceStatus';
import ExportButton from '@/components/ExportButton';
import FeedbackButton from '@/components/FeedbackButton';
import VoiceCapture from '@/components/VoiceCapture';
import { useCommandWebSocket } from '@/hooks/useCommandWebSocket';
import { useCommandStore } from '@/store/commandStore';

export default function Dashboard() {
  const [draftCommand, setDraftCommand] = useState<string>('Make the room feel fresh and light.');
  const isExecuting = useCommandStore((state) => state.isExecuting);
  const currentCommand = useCommandStore((state) => state.currentCommand);
  const ttsAudioBase64 = currentCommand?.ttsAudioBase64 ?? null;

  const { isConnected, connectionStatus, sendExecuteCommand, requestDeviceStatus, stopDevice } =
    useCommandWebSocket({
      autoConnect: true,
    });

  const canExecute = draftCommand.trim().length > 0 && !isExecuting && isConnected;

  return (
    <div className="dashboard-grid">
      <section className="panel panel-voice">
        <header className="panel-header">
          <h2>Voice Control</h2>
          <ConnectionStatus status={connectionStatus} />
        </header>

        <VoiceCapture onFinalTranscription={setDraftCommand} />

        <label className="field">
          <span>Command Draft</span>
          <textarea
            value={draftCommand}
            onChange={(event) => setDraftCommand(event.target.value)}
            placeholder="Example: Set lemon scent, intensity 7, duration 20 minutes."
            rows={4}
          />
        </label>

        <div className="button-row">
          <button
            type="button"
            className="btn-primary"
            disabled={!canExecute}
            onClick={() => {
              sendExecuteCommand(draftCommand.trim());
            }}
          >
            {isExecuting ? 'Executing...' : 'Execute Command'}
          </button>

          <button type="button" className="btn-secondary" onClick={requestDeviceStatus}>
            Refresh Status
          </button>

          <button type="button" className="btn-danger" onClick={stopDevice}>
            Stop Device
          </button>
        </div>

        <AudioPlayer audioBase64={ttsAudioBase64} />
      </section>

      <section className="panel panel-visual">
        <header className="panel-header">
          <h2>Device Visualization</h2>
          <span className="badge badge-neutral">Realtime</span>
        </header>
        <DeviceStatus />
        <AromatherapyCharts />
      </section>

      <section className="panel panel-command">
        <CommandPanel />
        <FeedbackButton commandId={currentCommand?.commandId ?? null} />
        <ExportButton />
      </section>
    </div>
  );
}
