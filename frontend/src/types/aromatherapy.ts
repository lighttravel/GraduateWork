/**
 * TypeScript type definitions for aromatherapy system.
 * Matches backend data models and API contracts.
 */

// ============== Scent Types ==============

export type ScentType = 'lemon' | 'lavender' | 'woody' | 'floral' | 'mixed';

export type ReleaseRhythm = 'gradual' | 'pulse' | 'intermittent';

export type CommandStatus = 'pending' | 'executed' | 'failed';

// ============== Control Parameters ==============

export interface MixingRatios {
  lemon?: number;
  lavender?: number;
  woody?: number;
  floral?: number;
}

export interface ControlJson {
  scent_type: ScentType;
  intensity: number; // 1-10
  duration_minutes: number; // 5-120
  release_rhythm: ReleaseRhythm;
  mixing_ratios?: MixingRatios;
}

// ============== Command Models ==============

export interface Command {
  id: string;
  created_at: string;
  user_input_text: string;
  llm_response_text: string | null;
  control_json: ControlJson;
  tts_audio_url: string | null;
  status: CommandStatus;
  execution_error: string | null;
  user_feedback: number | null; // 1-5 stars
  updated_at: string;
}

export interface CommandCreate {
  user_input_text: string;
}

export interface FeedbackUpdate {
  user_feedback: number; // 1-5
}

// ============== Device Status ==============

export interface DeviceStatus {
  is_active: boolean;
  current_scent: ScentType | null;
  current_intensity: number;
  remaining_minutes: number;
  control_params: ControlJson | Record<string, never>;
  error: string | null;
}

// ============== WebSocket Events ==============

export type WSEventType =
  | 'llm_processing'
  | 'command_generated'
  | 'command_saved'
  | 'device_executing'
  | 'device_executed'
  | 'tts_generating'
  | 'tts_ready'
  | 'execution_complete'
  | 'execution_error'
  | 'device_status'
  | 'device_stopped'
  | 'command_result'
  | 'error';

export interface WSEvent<T = any> {
  type: WSEventType;
  data?: T;
  message?: string;
}

// ============== WebSocket Messages (Client → Server) ==============

export interface ExecuteCommandMessage {
  type: 'execute_command';
  user_input: string;
}

export interface GetStatusMessage {
  type: 'get_status';
}

export interface StopDeviceMessage {
  type: 'stop_device';
}

export type WSClientMessage =
  | ExecuteCommandMessage
  | GetStatusMessage
  | StopDeviceMessage;

// ============== API Response Types ==============

export interface TTSRequest {
  text: string;
  voice?: string; // xiaoyan, aisjiuxu, etc.
  speed?: number; // 0-100
  volume?: number; // 0-100
  return_base64?: boolean;
}

export interface TTSResponse {
  audio_base64: string;
  size_bytes: number;
}

export interface VoiceInfo {
  name: string;
  language: string;
  gender: 'male' | 'female';
  description: string;
}

export interface VoicesResponse {
  voices: VoiceInfo[];
  default: string;
}

// ============== ASR Types ==============

export type ASREventType = 'partial' | 'final' | 'error';

export interface ASREvent {
  type: ASREventType;
  text?: string;
  message?: string;
}

// ============== UI State Types ==============

export interface TranscriptionState {
  partial: string;
  final: string;
  isRecording: boolean;
  error: string | null;
}

export interface ExecutionState {
  isExecuting: boolean;
  currentStep: string | null;
  error: string | null;
}
