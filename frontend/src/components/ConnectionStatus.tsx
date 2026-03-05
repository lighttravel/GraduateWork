export type ConnectionState = 'connected' | 'reconnecting' | 'disconnected';

interface ConnectionStatusProps {
  status: ConnectionState;
}

const LABEL_MAP: Record<ConnectionState, string> = {
  connected: 'WS Connected',
  reconnecting: 'WS Reconnecting',
  disconnected: 'WS Disconnected',
};

export default function ConnectionStatus({ status }: ConnectionStatusProps) {
  return (
    <div className={`connection-status connection-${status}`} aria-live="polite">
      <span className="connection-dot" aria-hidden="true" />
      <span>{LABEL_MAP[status]}</span>
    </div>
  );
}
