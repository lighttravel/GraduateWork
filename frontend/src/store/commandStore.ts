/**
 * Global state management using Zustand.
 * Stores command history, device status, and execution state.
 */
import { create } from 'zustand';
import type {
  Command,
  DeviceStatus,
  ControlJson,
  WSEvent,
} from '@/types/aromatherapy';

interface CommandStore {
  // Current device status
  deviceStatus: DeviceStatus | null;

  // Latest command
  currentCommand: {
    commandId: string | null;
    userInput: string;
    controlJson: ControlJson | null;
    responseText: string | null;
    ttsAudioBase64: string | null;
  } | null;

  // Command history
  commandHistory: Command[];

  // Execution state
  isExecuting: boolean;
  currentStep: string | null;
  executionError: string | null;

  // Recent WebSocket events (for debugging)
  recentEvents: WSEvent[];

  // Actions
  setDeviceStatus: (status: DeviceStatus) => void;
  setCurrentCommand: (command: {
    commandId: string | null;
    userInput: string;
    controlJson: ControlJson | null;
    responseText: string | null;
    ttsAudioBase64: string | null;
  }) => void;
  addCommandToHistory: (command: Command) => void;
  setIsExecuting: (isExecuting: boolean) => void;
  setCurrentStep: (step: string | null) => void;
  setExecutionError: (error: string | null) => void;
  addEvent: (event: WSEvent) => void;
  clearCurrentCommand: () => void;
  reset: () => void;
}

const initialState = {
  deviceStatus: null,
  currentCommand: null,
  commandHistory: [],
  isExecuting: false,
  currentStep: null,
  executionError: null,
  recentEvents: [],
};

export const useCommandStore = create<CommandStore>((set) => ({
  ...initialState,

  setDeviceStatus: (status) =>
    set({ deviceStatus: status }),

  setCurrentCommand: (command) =>
    set({ currentCommand: command }),

  addCommandToHistory: (command) =>
    set((state) => ({
      commandHistory: [command, ...state.commandHistory].slice(0, 50), // Keep last 50
    })),

  setIsExecuting: (isExecuting) =>
    set({ isExecuting }),

  setCurrentStep: (step) =>
    set({ currentStep: step }),

  setExecutionError: (error) =>
    set({ executionError: error }),

  addEvent: (event) =>
    set((state) => ({
      recentEvents: [...state.recentEvents, event].slice(-20), // Keep last 20 events
    })),

  clearCurrentCommand: () =>
    set({
      currentCommand: null,
      currentStep: null,
      executionError: null,
    }),

  reset: () =>
    set(initialState),
}));
