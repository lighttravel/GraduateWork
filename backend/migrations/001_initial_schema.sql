-- Initial Schema: aromatherapy_commands table
-- Purpose: Store voice commands, LLM-generated control parameters, and user feedback

-- Enable UUID extension for primary keys
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Create aromatherapy_commands table
CREATE TABLE aromatherapy_commands (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    -- User input and LLM response
    user_input_text TEXT NOT NULL,
    llm_response_text TEXT,

    -- Control parameters (JSON format)
    control_json JSONB NOT NULL,
    -- Expected structure:
    -- {
    --   "scent_type": "lemon" | "lavender" | "woody" | "floral" | "mixed",
    --   "intensity": 1-10,
    --   "duration_minutes": 5-120,
    --   "release_rhythm": "gradual" | "pulse" | "intermittent",
    --   "mixing_ratios": {"lemon": 0.6, "lavender": 0.4}  -- optional
    -- }

    -- TTS audio
    tts_audio_url TEXT,

    -- Execution status
    status TEXT NOT NULL DEFAULT 'pending',
    -- Values: 'pending' | 'executed' | 'failed'

    execution_error TEXT,

    -- User feedback (1-5 stars)
    user_feedback INTEGER CHECK (user_feedback >= 1 AND user_feedback <= 5),

    -- Timestamps
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Create indexes for performance
CREATE INDEX idx_commands_created_at ON aromatherapy_commands(created_at DESC);
CREATE INDEX idx_commands_status ON aromatherapy_commands(status);
CREATE INDEX idx_commands_scent_type ON aromatherapy_commands((control_json->>'scent_type'));

-- Create updated_at trigger
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_aromatherapy_commands_updated_at
BEFORE UPDATE ON aromatherapy_commands
FOR EACH ROW
EXECUTE FUNCTION update_updated_at_column();

-- Add comments for documentation
COMMENT ON TABLE aromatherapy_commands IS 'Stores voice commands with LLM-generated control parameters for aromatherapy device';
COMMENT ON COLUMN aromatherapy_commands.user_input_text IS 'Original user voice input transcribed by ASR';
COMMENT ON COLUMN aromatherapy_commands.llm_response_text IS 'LLM natural language response/confirmation';
COMMENT ON COLUMN aromatherapy_commands.control_json IS 'Structured control parameters (scent, intensity, duration, rhythm, mixing)';
COMMENT ON COLUMN aromatherapy_commands.status IS 'Execution status: pending, executed, or failed';
COMMENT ON COLUMN aromatherapy_commands.user_feedback IS 'User rating 1-5 stars for quality assessment';
