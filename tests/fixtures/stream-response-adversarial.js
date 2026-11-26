const encoder = new TextEncoder();

const triggers = {
  defaultInstalled: false,
  defaultCalls: 0,
  defaultDetached: false,
  byteInstalled: false,
  byteCalls: 0,
  byteDetached: false,
  statusTextCalls: 0,
  statusTextDetached: false,
};

export default {
  fetch(request) {
    const pathname = new URL(request.url).pathname;
    if (pathname === '/case-default-getter-detach') {
      const buf0 = encoder.encode('alpha-').buffer;
      const chunk0 = new Uint8Array(buf0);
      const chunk1 = encoder.encode('bravo-');
      const chunk2 = encoder.encode('charlie');
      const stream = new ReadableStream({
        start(controller) {
          controller.enqueue(chunk0);
          controller.enqueue(chunk1);
          controller.enqueue(chunk2);
          controller.close();
        },
      });
      const elements = stream._readableStreamController?._queue?._front?._elements;
      if (!Array.isArray(elements) || !elements[1] || typeof elements[1] !== 'object') {
        return new Response('missing default stream queue entry', { status: 500 });
      }
      triggers.defaultInstalled = true;
      Object.defineProperty(elements[1], 'value', {
        configurable: true,
        get() {
          triggers.defaultCalls += 1;
          const wasAttached = buf0.byteLength > 0;
          if (wasAttached) {
            structuredClone(buf0, { transfer: [buf0] });
          }
          triggers.defaultDetached = wasAttached && buf0.byteLength === 0;
          return chunk1;
        },
      });
      return new Response(stream);
    }

    if (pathname === '/case-byte-getter-detach') {
      const backing0 = new Uint8Array([0xff, ...encoder.encode('delta-'), 0xfe]);
      const backing1 = new Uint8Array([0xfd, ...encoder.encode('echo'), 0xfc]);
      const slice0 = new Uint8Array(backing0.buffer, 1, 6);
      const slice1 = new Uint8Array(backing1.buffer, 1, 4);
      const stream = new ReadableStream({
        type: 'bytes',
        start(controller) {
          controller.enqueue(slice0);
          controller.enqueue(slice1);
          controller.close();
        },
      });
      const elements = stream._readableStreamController?._queue?._front?._elements;
      if (
        !Array.isArray(elements) ||
        !elements[0] ||
        !elements[1] ||
        typeof elements[1] !== 'object' ||
        !(elements[0].buffer instanceof ArrayBuffer) ||
        !(elements[1].buffer instanceof ArrayBuffer)
      ) {
        return new Response('missing byte stream queue entry', { status: 500 });
      }
      const liveBuf0 = elements[0].buffer;
      const liveBuf1 = elements[1].buffer;
      triggers.byteInstalled = true;
      Object.defineProperty(elements[1], 'buffer', {
        configurable: true,
        get() {
          triggers.byteCalls += 1;
          const wasAttached = liveBuf0.byteLength > 0;
          if (wasAttached) {
            structuredClone(liveBuf0, { transfer: [liveBuf0] });
          }
          triggers.byteDetached = wasAttached && liveBuf0.byteLength === 0;
          return liveBuf1;
        },
      });
      return new Response(stream);
    }

    if (pathname === '/case-status-text-detach') {
      const chunk0 = encoder.encode('status-text-detached-chunk');
      const buf0 = chunk0.buffer;
      const stream = new ReadableStream({
        start(controller) {
          controller.enqueue(chunk0);
          controller.close();
        },
      });
      const response = new Response(stream, { status: 200 });
      Object.defineProperty(response, 'statusText', {
        configurable: true,
        get() {
          triggers.statusTextCalls += 1;
          const wasAttached = buf0.byteLength > 0;
          if (wasAttached) {
            structuredClone(buf0, { transfer: [buf0] });
          }
          triggers.statusTextDetached = wasAttached && buf0.byteLength === 0;
          return 'OK';
        },
      });
      return response;
    }

    if (pathname === '/case-verify-triggers') {
      return Response.json(triggers);
    }

    if (pathname === '/case-empty-closed') {
      const defaultStream = new ReadableStream({
        start(controller) {
          controller.close();
        },
      });
      const byteStream = new ReadableStream({
        type: 'bytes',
        start(controller) {
          controller.close();
        },
      });
      if (defaultStream.locked || byteStream.locked) {
        return new Response('unexpected initial lock state', { status: 500 });
      }
      return new Response(defaultStream);
    }

    if (pathname === '/case-empty-byte-closed') {
      return new Response(
        new ReadableStream({
          type: 'bytes',
          start(controller) {
            controller.close();
          },
        }),
      );
    }

    if (pathname === '/case-multi-chunk-credit') {
      const stream = new ReadableStream({
        start(controller) {
          for (let chunkIdx = 0; chunkIdx < 4; ++chunkIdx) {
            const chunk = new Uint8Array(6000);
            for (let i = 0; i < chunk.length; ++i) {
              chunk[i] = (chunkIdx * 37 + i) & 0xff;
            }
            controller.enqueue(chunk);
          }
          controller.close();
        },
      });
      return new Response(stream);
    }

    return new Response(`unknown case: ${pathname}`, { status: 404 });
  },
};
