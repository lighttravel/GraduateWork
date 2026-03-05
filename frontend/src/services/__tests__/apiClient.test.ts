import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { WebSocketManager, getWebSocketURL } from '@/services/apiClient';

class MockWebSocket {
  static CONNECTING = 0;
  static OPEN = 1;
  static CLOSING = 2;
  static CLOSED = 3;

  readyState = MockWebSocket.OPEN;
  sent: unknown[] = [];
  onopen: ((event: Event) => void) | null = null;
  onmessage: ((event: MessageEvent) => void) | null = null;
  onerror: ((event: Event) => void) | null = null;
  onclose: ((event: CloseEvent) => void) | null = null;

  constructor(public readonly url: string) {}

  send(data: unknown): void {
    this.sent.push(data);
  }

  close(): void {
    this.readyState = MockWebSocket.CLOSED;
  }

  emitClose(): void {
    this.readyState = MockWebSocket.CLOSED;
    this.onclose?.(new Event('close') as CloseEvent);
  }
}

describe('apiClient websocket utilities', () => {
  const sockets: MockWebSocket[] = [];

  beforeEach(() => {
    sockets.length = 0;
    vi.useFakeTimers();

    const webSocketConstructor = vi.fn((url: string) => {
      const socket = new MockWebSocket(url);
      sockets.push(socket);
      return socket as unknown as WebSocket;
    }) as unknown as typeof WebSocket;

    Object.assign(webSocketConstructor, {
      CONNECTING: MockWebSocket.CONNECTING,
      OPEN: MockWebSocket.OPEN,
      CLOSING: MockWebSocket.CLOSING,
      CLOSED: MockWebSocket.CLOSED,
    });

    vi.stubGlobal('WebSocket', webSocketConstructor);
  });

  afterEach(() => {
    vi.runOnlyPendingTimers();
    vi.useRealTimers();
    vi.unstubAllGlobals();
  });

  it('builds websocket url with /api prefix', () => {
    const url = getWebSocketURL('/ws/commands');
    expect(url).toMatch(/^ws:\/\//);
    expect(url).toContain('/api/ws/commands');
  });

  it('sends json and binary payloads', () => {
    const manager = new WebSocketManager('/ws/test');
    manager.connect();

    manager.send({ type: 'ping', value: 1 });
    manager.send(new Uint8Array([1, 2, 3]));

    expect(sockets).toHaveLength(1);
    expect(sockets[0].sent[0]).toBe('{"type":"ping","value":1}');
    expect(sockets[0].sent[1]).toEqual(new Uint8Array([1, 2, 3]));
  });

  it('attempts reconnect after unintentional close', () => {
    const manager = new WebSocketManager('/ws/test', {
      reconnectDelay: 10,
      maxReconnectAttempts: 1,
    });

    manager.connect();
    expect(sockets).toHaveLength(1);

    sockets[0].emitClose();
    vi.advanceTimersByTime(10);

    expect(sockets).toHaveLength(2);
  });

  it('does not reconnect when closed intentionally', () => {
    const manager = new WebSocketManager('/ws/test', {
      reconnectDelay: 10,
      maxReconnectAttempts: 1,
    });

    manager.connect();
    expect(sockets).toHaveLength(1);

    manager.close();
    sockets[0].emitClose();
    vi.runAllTimers();

    expect(sockets).toHaveLength(1);
  });
});
