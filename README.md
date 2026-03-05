# AI-Powered Voice-Controlled Aromatherapy System

A full-stack web application that enables natural language voice control of aromatherapy devices using AI-powered intent parsing.

## System Architecture

```
Voice Input → iFlytek ASR → GLM-4.7 LLM → JSON Control Commands → Visualization + TTS Response
```

### Key Features

- 🎙️ **Voice Control**: Push-to-talk voice input with real-time transcription (iFlytek ASR)
- 🤖 **AI Intent Parsing**: Natural language understanding via GLM-4.7 LLM
- 📊 **Real-Time Visualization**: Dynamic dashboard showing device state (scent, intensity, duration, rhythm)
- 🔊 **Voice Feedback**: TTS confirmation of commands (iFlytek TTS)
- 🌐 **Multi-Client Sync**: Real-time state synchronization across multiple dashboards
- 📈 **Data Collection**: Training data export for model fine-tuning

## Tech Stack

### Backend
- **FastAPI** - Web framework
- **PostgreSQL** - Database (with JSONB support)
- **iFlytek** - ASR & TTS services
- **GLM-4.7** - LLM (Anthropic-compatible API)
- **WebSocket** - Real-time bidirectional communication

### Frontend (Coming in Phase 6)
- **React** + **TypeScript** - UI framework
- **Vite** - Build tool
- **Recharts** - Visualization
- **Socket.IO** - WebSocket client

## Project Structure

```
test1/
├── backend/
│   ├── main.py                      # FastAPI app entry
│   ├── config.py                    # Configuration management
│   ├── database.py                  # Database connection
│   ├── models.py                    # Data models
│   ├── requirements.txt             # Python dependencies
│   ├── .env.example                 # Environment template
│   ├── migrations/
│   │   └── 001_initial_schema.sql   # Database schema
│   ├── repositories/
│   │   └── command_repository.py    # Data access layer
│   ├── services/                    # Business logic (Phase 2-4)
│   ├── routers/                     # API endpoints (Phase 3-5)
│   └── tests/                       # Unit & integration tests
├── frontend/                        # React app (Phase 6+)
└── README.md
```

## Quick Start

### Prerequisites

- Python 3.11+
- PostgreSQL 15+
- Node.js 20+ (for frontend)

### Backend Setup

1. **Clone the repository**
   ```bash
   git clone <repository-url>
   cd test1/backend
   ```

2. **Create virtual environment**
   ```bash
   python -m venv venv
   source venv/bin/activate  # Windows: venv\Scripts\activate
   ```

3. **Install dependencies**
   ```bash
   pip install -r requirements.txt
   ```

4. **Configure environment**
   ```bash
   cp .env.example .env
   # Edit .env with your credentials
   ```

5. **Initialize database**
   ```bash
   psql -U postgres -c "CREATE DATABASE aromatherapy_db;"
   psql -U postgres -d aromatherapy_db -f migrations/001_initial_schema.sql
   ```

6. **Run the server**
   ```bash
   python main.py
   # Or with uvicorn:
   uvicorn main:app --reload --host 0.0.0.0 --port 8000
   ```

7. **Access the API**
   - API Docs: http://localhost:8000/docs
   - Health Check: http://localhost:8000/health

## Configuration

All configuration is managed via environment variables in `.env`:

| Variable | Description | Required |
|----------|-------------|----------|
| `DATABASE_URL` | PostgreSQL connection string | ✅ |
| `IFLYTEK_ASR_APPID` | iFlytek ASR App ID | ✅ |
| `IFLYTEK_ASR_API_SECRET` | iFlytek ASR API Secret | ✅ |
| `IFLYTEK_ASR_API_KEY` | iFlytek ASR API Key | ✅ |
| `IFLYTEK_TTS_APPID` | iFlytek TTS App ID | ✅ |
| `IFLYTEK_TTS_API_SECRET` | iFlytek TTS API Secret | ✅ |
| `IFLYTEK_TTS_API_KEY` | iFlytek TTS API Key | ✅ |
| `ANTHROPIC_AUTH_TOKEN` | GLM-4.7 API Token | ✅ |
| `ANTHROPIC_BASE_URL` | GLM-4.7 API Base URL | ✅ |
| `CORS_ORIGINS` | Allowed frontend origins | ❌ |
| `APP_ENV` | Environment (development/production) | ❌ |
| `LOG_LEVEL` | Logging level (DEBUG/INFO/WARNING/ERROR) | ❌ |

**Security Note**: NEVER commit `.env` to version control. API credentials are for development/testing only.

## Development Roadmap

- [x] **Phase 1**: Backend Foundation (FastAPI, DB, config) ✅
- [ ] **Phase 2**: LLM Intent Parsing (GLM-4.7 integration)
- [ ] **Phase 3**: iFlytek ASR Integration (WebSocket)
- [ ] **Phase 4**: iFlytek TTS Integration (WebSocket)
- [ ] **Phase 5**: Command Execution Pipeline
- [ ] **Phase 6**: Frontend Foundation (React + TypeScript)
- [ ] **Phase 7**: Voice Capture Component (push-to-talk)
- [ ] **Phase 8**: Visualization Dashboard (charts, device state)
- [ ] **Phase 9**: Audio Playback Component
- [ ] **Phase 10**: Real-Time Multi-Client Sync (WebSocket rooms)
- [ ] **Phase 11**: Data Collection & Export
- [ ] **Phase 12**: Error Handling & Edge Cases
- [ ] **Phase 13**: Testing (80%+ coverage)

## API Endpoints (Current)

### Health Check
```http
GET /health
```

Returns service status and configuration info.

**Response:**
```json
{
  "status": "healthy",
  "environment": "development",
  "services": {
    "database": "connected",
    "iflytek_asr": "configured",
    "iflytek_tts": "configured",
    "llm": "configured"
  }
}
```

## Database Schema

### `aromatherapy_commands` Table

| Column | Type | Description |
|--------|------|-------------|
| `id` | UUID | Primary key |
| `created_at` | TIMESTAMPTZ | Command creation timestamp |
| `user_input_text` | TEXT | Original voice input (transcribed) |
| `llm_response_text` | TEXT | LLM natural language response |
| `control_json` | JSONB | Structured control parameters |
| `tts_audio_url` | TEXT | URL to TTS audio file |
| `status` | TEXT | Execution status (pending/executed/failed) |
| `execution_error` | TEXT | Error message if failed |
| `user_feedback` | INTEGER | User rating (1-5 stars) |
| `updated_at` | TIMESTAMPTZ | Last update timestamp |

### Control JSON Schema

```json
{
  "scent_type": "lemon | lavender | woody | floral | mixed",
  "intensity": 1-10,
  "duration_minutes": 5-120,
  "release_rhythm": "gradual | pulse | intermittent",
  "mixing_ratios": {
    "lemon": 0.6,
    "lavender": 0.4
  }
}
```

## Testing

```bash
# Run all tests
pytest

# Run with coverage
pytest --cov=. --cov-report=html

# Run specific test file
pytest tests/test_intent_parser.py
```

## License

MIT License - see LICENSE file for details

## Contributing

This is a graduate research project. Contributions are welcome via pull requests.
