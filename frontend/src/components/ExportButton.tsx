import { useState } from 'react';
import { apiClient } from '@/services/apiClient';

type ExportFormat = 'csv' | 'jsonl';

export default function ExportButton() {
  const [isExporting, setIsExporting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const exportTrainingData = async (format: ExportFormat) => {
    if (isExporting) {
      return;
    }

    setIsExporting(true);
    setError(null);

    try {
      const response = await apiClient.get('/export/training-data', {
        params: { format },
        responseType: 'blob',
      });

      const blob = new Blob([response.data], {
        type: format === 'csv' ? 'text/csv' : 'application/x-ndjson',
      });

      const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
      const filename = `training_data_${timestamp}.${format}`;
      const url = window.URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = filename;
      document.body.appendChild(link);
      link.click();
      link.remove();
      window.URL.revokeObjectURL(url);
    } catch {
      setError('Failed to export training data.');
    } finally {
      setIsExporting(false);
    }
  };

  return (
    <div className="export-box">
      <h3>Training Data Export</h3>
      <div className="button-row">
        <button
          type="button"
          className="btn-secondary"
          disabled={isExporting}
          onClick={() => {
            void exportTrainingData('csv');
          }}
        >
          Export CSV
        </button>
        <button
          type="button"
          className="btn-secondary"
          disabled={isExporting}
          onClick={() => {
            void exportTrainingData('jsonl');
          }}
        >
          Export JSONL
        </button>
      </div>
      {error ? <p className="voice-error-text">{error}</p> : null}
    </div>
  );
}
