/**
 * API Client for aromatherapy backend.
 * Provides HTTP and WebSocket communication utilities.
 */
import axios, { AxiosInstance } from 'axios';

// Base URL for API (proxied by Vite in development)
const API_BASE_URL = import.meta.env.VITE_API_BASE_URL || 'http://localhost:8000';

/**
 * Axios instance for HTTP requests.
 * Configured with base URL and default headers.
 */
export const apiClient: AxiosInstance = axios.create({
  baseURL: `${API_BASE_URL}/api`,
  timeout: 30000, // 30 seconds
  headers: {
    'Content-Type': 'application/json',
  },
});

// Request interceptor (for future auth tokens)
apiClient.interceptors.request.use(
  (config) => {
    // Future: Add authentication token here
    // const token = localStorage.getItem('auth_token');
    // if (token) {
    //   config.headers.Authorization = `Bearer ${token}`;
    // }
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

// Response interceptor (for error handling)
apiClient.interceptors.response.use(
  (response) => response,
  (error) => {
    // Log errors for debugging
    console.error('API Error:', error.response?.data || error.message);
    return Promise.reject(error);
  }
);

/**
 * Get WebSocket URL for the given endpoint.
 */
export const getWebSocketURL = (endpoint: string): string => {
  const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = import.meta.env.VITE_WS_HOST || 'localhost:8000';
  return `${wsProtocol}//${host}/api${endpoint}`;
};

export interface WebSocketManagerOptions {
  autoReconnect?: boolean;
  maxReconnectAttempts?: number;
  reconnectDelay?: number;
}

/**
 * WebSocket connection manager.
 * Handles connection lifecycle and reconnection logic.
 */
export class WebSocketManager {
  private ws: WebSocket | null = null;
  private url: string;
  private autoReconnect: boolean;
  private reconnectAttempts = 0;
  private maxReconnectAttempts: number;
  private reconnectDelay: number;
  private reconnectTimer: number | null = null;
  private isIntentionallyClosed = false;

  // Event handlers
  public onOpen: (() => void) | null = null;
  public onMessage: ((event: MessageEvent) => void) | null = null;
  public onError: ((error: Event) => void) | null = null;
  public onClose: (() => void) | null = null;

  constructor(endpoint: string, options: WebSocketManagerOptions = {}) {
    this.url = getWebSocketURL(endpoint);
    this.autoReconnect = options.autoReconnect ?? true;
    this.maxReconnectAttempts = options.maxReconnectAttempts ?? 5;
    this.reconnectDelay = options.reconnectDelay ?? 2000;
  }

  /**
   * Connect to WebSocket server.
   */
  connect(): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      console.warn('WebSocket already connected');
      return;
    }

    this.isIntentionallyClosed = false;
    console.log(`Connecting to WebSocket: ${this.url}`);

    try {
      this.ws = new WebSocket(this.url);

      this.ws.onopen = () => {
        console.log('WebSocket connected');
        this.reconnectAttempts = 0;
        if (this.onOpen) this.onOpen();
      };

      this.ws.onmessage = (event) => {
        if (this.onMessage) this.onMessage(event);
      };

      this.ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        if (this.onError) this.onError(error);
      };

      this.ws.onclose = () => {
        console.log('WebSocket closed');
        if (this.onClose) this.onClose();

        // Attempt reconnection if not intentionally closed
        if (!this.isIntentionallyClosed && this.autoReconnect) {
          this.attemptReconnect();
        }
      };
    } catch (error) {
      console.error('Failed to create WebSocket:', error);
      this.attemptReconnect();
    }
  }

  /**
   * Send message to WebSocket server.
   */
  send(data: unknown): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      if (data instanceof Blob || data instanceof ArrayBuffer || ArrayBuffer.isView(data)) {
        this.ws.send(data);
        return;
      }

      const message = typeof data === 'string' ? data : JSON.stringify(data);
      this.ws.send(message);
    } else {
      console.error('WebSocket not connected. Cannot send message.');
    }
  }

  /**
   * Close WebSocket connection.
   */
  close(): void {
    this.isIntentionallyClosed = true;
    if (this.reconnectTimer !== null) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }
  }

  /**
   * Get current connection state.
   */
  get readyState(): number {
    return this.ws?.readyState ?? WebSocket.CLOSED;
  }

  /**
   * Check if WebSocket is connected.
   */
  get isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN;
  }

  /**
   * Attempt to reconnect with exponential backoff.
   */
  private attemptReconnect(): void {
    if (this.reconnectAttempts >= this.maxReconnectAttempts) {
      console.error('Max reconnection attempts reached. Giving up.');
      return;
    }

    this.reconnectAttempts++;
    const delay = this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1);

    console.log(
      `Reconnecting in ${delay}ms (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`
    );

    this.reconnectTimer = window.setTimeout(() => {
      this.connect();
    }, delay);
  }
}
