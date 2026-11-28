// Zero-dependency Capsid WebSocket showcase covering:
//   - Inbound WebSocketPair + 101 Upgrade (text, binary ArrayBuffer, custom close)
//   - Subprotocol negotiation (Sec-WebSocket-Protocol)
//   - Graceful half-open drain via server.accept({ allowHalfOpen: true })
//   - Buffered early frames before delayed server.accept()
//   - Default binaryType === "blob" binary frame handling + WebCrypto SHA-256
//   - Outbound new WebSocket(url, protocols) gated by permissions.fetch.allow
//   - HTTP Upgrade fallback (fetch with Upgrade: websocket receiving non-101/gzip)
//   - Fail-closed WebSocket API contract assertions

function requireUpgrade(request) {
  if (request.headers.get('upgrade')?.toLowerCase() !== 'websocket') {
    return new Response('Expected Upgrade: websocket request', { status: 426 });
  }
  return null;
}

function toHex(buffer) {
  const bytes = new Uint8Array(buffer);
  let out = '';
  for (let i = 0; i < bytes.length; ++i) {
    out += bytes[i].toString(16).padStart(2, '0');
  }
  return out;
}

function handleWsEcho(request) {
  const denied = requireUpgrade(request);
  if (denied) {
    return denied;
  }
  const [client, server] = Object.values(new WebSocketPair());
  const defaultBinaryType = server.binaryType;
  server.accept();
  server.binaryType = 'arraybuffer';

  server.addEventListener('message', (event) => {
    if (typeof event.data === 'string') {
      if (event.data === 'info') {
        server.send(
          JSON.stringify({
            defaultBinaryType,
            activeBinaryType: server.binaryType,
            readyState: server.readyState,
          }),
        );
        return;
      }
      if (event.data.startsWith('close:')) {
        const [, codeStr, ...reasonParts] = event.data.split(':');
        const code = Number(codeStr) || 1000;
        const reason = reasonParts.join(':');
        server.close(code, reason);
        return;
      }
      server.send(`echo:${event.data}`);
      return;
    }
    // Binary ArrayBuffer echo (byte-exact, including multi-KiB payloads)
    server.send(event.data);
  });

  return new Response(null, { status: 101, webSocket: client });
}

function handleWsSubprotocol(request) {
  const denied = requireUpgrade(request);
  if (denied) {
    return denied;
  }
  const offered = (request.headers.get('sec-websocket-protocol') || '')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean);
  const supported = ['capsid.chat.v1', 'capsid.json.v1'];
  const selected = offered.find((proto) => supported.includes(proto));

  const [client, server] = Object.values(new WebSocketPair());
  server.accept();
  server.addEventListener('message', (event) => {
    server.send(
      JSON.stringify({
        selected: selected ?? null,
        offered,
        received: event.data,
      }),
    );
  });

  const headers = new Headers();
  if (selected) {
    headers.set('sec-websocket-protocol', selected);
  }
  return new Response(null, { status: 101, headers, webSocket: client });
}

function handleWsHalfOpen(request) {
  const denied = requireUpgrade(request);
  if (denied) {
    return denied;
  }
  const [client, server] = Object.values(new WebSocketPair());
  server.accept({ allowHalfOpen: true });

  let messageCount = 0;
  server.addEventListener('message', (event) => {
    messageCount++;
    server.send(`ack:${messageCount}:${event.data}`);
  });
  server.addEventListener('close', (event) => {
    // Under allowHalfOpen, the server can still send a final message after
    // observing the client's clean Close before reciprocating Close.
    server.send(`final-after-close:${messageCount}:${event.code}:${event.reason}`);
    server.close(1000, 'half-open-complete');
  });

  return new Response(null, { status: 101, webSocket: client });
}

function handleWsDelayedAccept(request) {
  const denied = requireUpgrade(request);
  if (denied) {
    return denied;
  }
  const [client, server] = Object.values(new WebSocketPair());
  const received = [];

  server.addEventListener('message', (event) => {
    received.push(String(event.data));
    server.send(`buffered:${received.length}:${event.data}`);
  });

  // Deliberately delay accept() after returning 101; early client frames
  // remain buffered until accept() runs.
  setTimeout(() => {
    server.accept();
  }, 40);

  return new Response(null, { status: 101, webSocket: client });
}

function handleWsBlobMode(request) {
  const denied = requireUpgrade(request);
  if (denied) {
    return denied;
  }
  const [client, server] = Object.values(new WebSocketPair());
  server.accept();
  // Retain default binaryType === "blob" (workerd 2026-08-20 profile default).
  server.addEventListener('message', async (event) => {
    if (event.data instanceof Blob) {
      const buf = await event.data.arrayBuffer();
      const digest = await crypto.subtle.digest('SHA-256', buf);
      server.send(
        JSON.stringify({
          isBlob: true,
          binaryType: server.binaryType,
          size: buf.byteLength,
          sha256: toHex(digest),
        }),
      );
      return;
    }
    server.send(JSON.stringify({ isBlob: false, type: typeof event.data }));
  });

  return new Response(null, { status: 101, webSocket: client });
}

async function handleWsOutbound(request) {
  const url = new URL(request.url);
  const target = url.searchParams.get('target');
  const protocol = url.searchParams.get('protocol');
  if (!target) {
    return Response.json(
      { ok: false, error: 'provide ?target=ws://127.0.0.1:18099/echo' },
      { status: 400 },
    );
  }

  try {
    const result = await new Promise((resolve, reject) => {
      const ws = protocol ? new WebSocket(target, [protocol]) : new WebSocket(target);
      ws.binaryType = 'arraybuffer';
      const replies = [];
      let settled = false;

      const timer = setTimeout(() => {
        if (!settled) {
          settled = true;
          try {
            ws.close();
          } catch {
            // ignore
          }
          reject(new Error('outbound WebSocket timed out'));
        }
      }, 5000);

      const finish = (fn, val) => {
        if (settled) {
          return;
        }
        settled = true;
        clearTimeout(timer);
        fn(val);
      };

      ws.addEventListener('open', () => {
        ws.send('hello-outbound-text');
        ws.send(new Uint8Array([1, 2, 3, 4, 5]));
      });

      ws.addEventListener('message', (event) => {
        if (typeof event.data === 'string') {
          replies.push({ kind: 'text', value: event.data });
        } else if (event.data instanceof ArrayBuffer) {
          replies.push({ kind: 'binary', bytes: Array.from(new Uint8Array(event.data)) });
        }
        if (replies.length === 2) {
          ws.close(1000, 'outbound-done');
        }
      });

      ws.addEventListener('error', (event) => {
        finish(reject, new Error(event.message || 'outbound WebSocket failed'));
      });

      ws.addEventListener('close', (event) => {
        if (replies.length === 2) {
          finish(resolve, {
            ok: true,
            url: ws.url,
            protocol: ws.protocol,
            replies,
            closeCode: event.code,
            closeReason: event.reason,
            wasClean: event.wasClean,
          });
        } else {
          finish(reject, new Error(`closed early (${event.code}): ${event.reason}`));
        }
      });
    });

    return Response.json(result);
  } catch (err) {
    return Response.json(
      {
        ok: false,
        error: err?.message ?? String(err),
      },
      { status: 502 },
    );
  }
}

async function handleWsUpgradeFallback(request) {
  const target = new URL(request.url).searchParams.get('target');
  if (!target) {
    return Response.json(
      { ok: false, error: 'provide ?target=http://127.0.0.1:18099/fallback' },
      { status: 400 },
    );
  }
  const upstream = await fetch(target, {
    headers: {
      upgrade: 'websocket',
      'accept-encoding': 'gzip',
    },
  });
  const text = await upstream.text();
  return Response.json({
    ok: upstream.status === 200,
    status: upstream.status,
    contentEncoding: upstream.headers.get('content-encoding'),
    fallbackHeader: upstream.headers.get('x-upgrade-fallback'),
    body: text,
  });
}

function handleWsErrorCases() {
  const checks = {};

  // 1. Non-101 status with webSocket throws RangeError.
  const [c1] = Object.values(new WebSocketPair());
  try {
    new Response(null, { status: 200, webSocket: c1 });
    checks.non101Rejected = false;
  } catch (err) {
    checks.non101Rejected = err instanceof RangeError;
  }

  // 2. Non-empty body with 101 webSocket is rejected.
  const [c2] = Object.values(new WebSocketPair());
  try {
    new Response('not-allowed', { status: 101, webSocket: c2 });
    checks.nonEmptyBodyRejected = false;
  } catch (err) {
    checks.nonEmptyBodyRejected = err instanceof TypeError || err instanceof RangeError;
  }

  // 3. Cloning an upgraded 101 Response is rejected.
  const [c3] = Object.values(new WebSocketPair());
  const upgraded = new Response(null, { status: 101, webSocket: c3 });
  try {
    upgraded.clone();
    checks.cloneUpgradedRejected = false;
  } catch {
    checks.cloneUpgradedRejected = true;
  }

  // 4. Invalid close code (e.g. 1005 / 1006) throws on server.close().
  const [, s4] = Object.values(new WebSocketPair());
  s4.accept();
  let code1005Threw = false;
  let code1006Threw = false;
  try {
    s4.close(1005);
  } catch {
    code1005Threw = true;
  }
  try {
    s4.close(1006);
  } catch {
    code1006Threw = true;
  }
  checks.invalidCloseCodesRejected = code1005Threw && code1006Threw;

  // 5. Invalid binaryType assignment preserves current value.
  const prevType = s4.binaryType;
  s4.binaryType = 'invalid-mode';
  checks.invalidBinaryTypeIgnored = s4.binaryType === prevType;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

export default {
  async fetch(request) {
    const { pathname } = new URL(request.url);
    switch (pathname) {
      case '/':
        return Response.json({
          name: 'Capsid WebSocket Showcase (FetchRPC v4)',
          routes: [
            '/ws/echo',
            '/ws/subprotocol',
            '/ws/half-open',
            '/ws/delayed-accept',
            '/ws/blob-mode',
            '/ws/outbound',
            '/ws/upgrade-fallback',
            '/ws/error-cases',
          ],
        });
      case '/ws/echo':
        return handleWsEcho(request);
      case '/ws/subprotocol':
        return handleWsSubprotocol(request);
      case '/ws/half-open':
        return handleWsHalfOpen(request);
      case '/ws/delayed-accept':
        return handleWsDelayedAccept(request);
      case '/ws/blob-mode':
        return handleWsBlobMode(request);
      case '/ws/outbound':
        return handleWsOutbound(request);
      case '/ws/upgrade-fallback':
        return handleWsUpgradeFallback(request);
      case '/ws/error-cases':
        return handleWsErrorCases();
      default:
        return Response.json({ error: 'not found', pathname }, { status: 404 });
    }
  },
};
