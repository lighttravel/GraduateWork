import { useEffect, useState } from 'react';
import { BrowserRouter, Navigate, Route, Routes } from 'react-router-dom';
import Dashboard from '@/pages/Dashboard';

function AppShell() {
  const [isOnline, setIsOnline] = useState<boolean>(navigator.onLine);

  useEffect(() => {
    const handleOnline = () => setIsOnline(true);
    const handleOffline = () => setIsOnline(false);

    window.addEventListener('online', handleOnline);
    window.addEventListener('offline', handleOffline);

    return () => {
      window.removeEventListener('online', handleOnline);
      window.removeEventListener('offline', handleOffline);
    };
  }, []);

  return (
    <div className="app-shell">
      <header className="app-header">
        <div className="app-header-inner">
          <div className="brand">
            <div className="brand-mark" aria-hidden="true" />
            <div>
              <h1>Aromatherapy Command Console</h1>
              <p>AI voice-driven scent orchestration dashboard</p>
            </div>
          </div>
          <div className="status-chip" aria-live="polite">
            <span
              className={`status-dot ${isOnline ? 'status-dot-online' : 'status-dot-offline'}`}
              aria-hidden="true"
            />
            <span>{isOnline ? 'Network Online' : 'Network Offline'}</span>
          </div>
        </div>
      </header>

      <main className="app-main container">
        <Routes>
          <Route path="/" element={<Navigate to="/dashboard" replace />} />
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="*" element={<Navigate to="/dashboard" replace />} />
        </Routes>
      </main>
    </div>
  );
}

export default function App() {
  return (
    <BrowserRouter>
      <AppShell />
    </BrowserRouter>
  );
}
