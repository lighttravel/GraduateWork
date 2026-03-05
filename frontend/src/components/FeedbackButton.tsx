import { useState } from 'react';
import { apiClient } from '@/services/apiClient';

interface FeedbackButtonProps {
  commandId: string | null;
}

export default function FeedbackButton({ commandId }: FeedbackButtonProps) {
  const [selectedRating, setSelectedRating] = useState<number>(0);
  const [isSubmitting, setIsSubmitting] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);

  const submitFeedback = async (rating: number) => {
    if (!commandId || isSubmitting) {
      return;
    }

    setIsSubmitting(true);
    setError(null);
    setMessage(null);

    try {
      await apiClient.patch(`/commands/${commandId}/feedback`, {
        user_feedback: rating,
      });
      setSelectedRating(rating);
      setMessage(`Saved ${rating}-star feedback.`);
    } catch {
      setError('Failed to submit feedback.');
    } finally {
      setIsSubmitting(false);
    }
  };

  return (
    <div className="feedback-box">
      <h3>Feedback</h3>
      <div className="feedback-stars" role="group" aria-label="Rate latest command">
        {[1, 2, 3, 4, 5].map((rating) => (
          <button
            key={rating}
            type="button"
            className={`star-button ${selectedRating >= rating ? 'star-selected' : ''}`}
            disabled={!commandId || isSubmitting}
            onClick={() => {
              void submitFeedback(rating);
            }}
            aria-label={`Rate ${rating} star${rating > 1 ? 's' : ''}`}
          >
            ★
          </button>
        ))}
      </div>
      {!commandId ? <p className="muted">Run a command first to enable feedback.</p> : null}
      {message ? <p className="feedback-success">{message}</p> : null}
      {error ? <p className="voice-error-text">{error}</p> : null}
    </div>
  );
}
