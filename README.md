# AI-Powered Voice-Controlled Aromatherapy System

Full-stack web application for voice-driven aromatherapy control:

`Voice Input -> iFlytek ASR -> GLM-4.7 LLM -> JSON Control -> Device Visualization + TTS`

## Status

All planned phases (1-13) are implemented in this repository, including:

- Backend foundation, ASR/TTS/LLM integration, command execution pipeline
- Frontend dashboard, voice capture, realtime device sync, audio playback
- Feedback and training-data export endpoints
- Global backend error handlers and frontend error boundary/connection status
- Backend + frontend automated tests and E2E scaffold

## Tech Stack

### Backend
- FastAPI
- SQLAlchemy (async) + PostgreSQL (JSONB)
- iFlytek ASR/TTS WebSocket APIs
- GLM-4.7 via Anthropic-compatible API
- Native WebSocket for realtime events

### Frontend
- React 18 + TypeScript + Vite
- Zustand state management
- Axios
- Recharts
- Native WebSocket client manager
- Vitest + Testing Library
- Playwright (E2E)

## Project Structure

```text
test1/
├── backend/
│   ├── main.py
│   ├── config.py
│   ├── database.py
│   ├── models.py
│   ├── requirements.txt
│   ├── migrations/
│   ├── middleware/
│   ├── repositories/
│   ├── routers/
│   ├── services/
│   └── tests/
├── frontend/
│   ├── src/
│   │   ├── components/
│   │   ├── hooks/
│   │   ├── pages/
│   │   ├── services/
│   │   ├── store/
│   │   └── types/
│   ├── e2e/
│   └── package.json
├── PROJECT_HANDOFF.md
├── EXECUTION_GUIDE.md
└── README.md
```

## Quick Start

## 1) Backend

```bash
cd backend
python -m venv venv
# Windows:
venv\Scripts\activate
# Linux/macOS:
# source venv/bin/activate

pip install -r requirements.txt
```

Create `.env` from `.env.example` and fill required keys:

- `DATABASE_URL`
- `IFLYTEK_ASR_*`
- `IFLYTEK_TTS_*`
- `ANTHROPIC_AUTH_TOKEN`
- `ANTHROPIC_BASE_URL`

Initialize DB:

```bash
psql -U postgres -c "CREATE DATABASE aromatherapy_db;"
psql -U postgres -d aromatherapy_db -f migrations/001_initial_schema.sql
```

Run backend:

```bash
uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

## 2) Frontend

```bash
cd frontend
npm install
npm run dev
```

Frontend default URL: `http://localhost:5173`  
Backend docs: `http://localhost:8000/docs`  
Health check: `http://localhost:8000/health`

## Optional: Local Proxy (example `127.0.0.1:7897`)

If package installs fail due network restrictions, configure proxy for the terminal session:

```powershell
$env:HTTP_PROXY='http://127.0.0.1:7897'
$env:HTTPS_PROXY='http://127.0.0.1:7897'
```

For Conda:

```powershell
conda config --set proxy_servers.http http://127.0.0.1:7897
conda config --set proxy_servers.https http://127.0.0.1:7897
```

## API Endpoints

### HTTP
- `GET /health`
- `POST /api/tts`
- `GET /api/tts/voices`
- `PATCH /api/commands/{command_id}/feedback`
- `GET /api/export/training-data?format=csv|jsonl`

### WebSocket
- `WS /api/ws/asr`
- `WS /api/ws/commands`

## Realtime Command Event Types

- `llm_processing`
- `command_generated`
- `command_saved`
- `device_executing`
- `device_executed`
- `tts_generating`
- `tts_ready`
- `execution_complete`
- `execution_error`
- `device_status`
- `device_stopped`
- `command_result`

## Testing

### Backend

From repo root:

```bash
python -m pytest -q backend/tests
```

Or from backend directory:

```bash
cd backend
pytest -q tests
```

### Frontend

```bash
cd frontend
npm run test:run
npm run build
```

### E2E

List tests (no browser install required):

```bash
cd frontend
npm run test:e2e -- --list
```

Run E2E (requires Playwright browsers installed):

```bash
npm run test:e2e
```

## License

MIT License.
