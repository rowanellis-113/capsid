// Binding RPC v2 (v0.4.0): tamper-proof internal JS bridge for stream proxies,
// controlled Request/Response transfer, and late-result resource disposal.
//
// Evaluated once during worker bootstrap BEFORE any application or Binding
// package code runs, capturing intrinsic constructors, prototypes, and brand-
// checked accessors privately. Registered via the one-shot core hook
// `capsidInstallBindingBridge` and never exposed on globalThis.

import {
  bridgeHeaderPairs,
  createHeadersFromIncomingPairs,
  consumeFastResponseBody,
  getFastStreamResponseBody,
  isNullFastResponseBody,
} from './fetch.js';
import { getResponseWebSocket } from './websocket.js';

const ownDesc = Object.getOwnPropertyDescriptor;
const getProto = Object.getPrototypeOf;
const objectHasOwn = Object.hasOwn;
const IntrinsicPromise = Promise;
const promiseThen = IntrinsicPromise.prototype.then;
const promiseResolve = IntrinsicPromise.resolve.bind(IntrinsicPromise);

const IntrinsicReadableStream = ReadableStream;
const IntrinsicWritableStream = WritableStream;
const IntrinsicRequest = Request;
const IntrinsicResponse = Response;
const IntrinsicHeaders = Headers;
const IntrinsicDOMException = DOMException;
const IntrinsicUint8Array = Uint8Array;
const IntrinsicArrayBuffer = ArrayBuffer;
const arrayBufferIsView = IntrinsicArrayBuffer.isView;
const TypedArrayProto = getProto(IntrinsicUint8Array.prototype);
const typedBuffer = ownDesc(TypedArrayProto, 'buffer').get;
const typedByteOffset = ownDesc(TypedArrayProto, 'byteOffset').get;
const typedByteLength = ownDesc(TypedArrayProto, 'byteLength').get;
const dataViewProto = DataView.prototype;
const dataViewBuffer = ownDesc(dataViewProto, 'buffer').get;
const dataViewByteOffset = ownDesc(dataViewProto, 'byteOffset').get;
const dataViewByteLength = ownDesc(dataViewProto, 'byteLength').get;
const arrayBufferByteLength = ownDesc(IntrinsicArrayBuffer.prototype, 'byteLength').get;

const readableLockedGetter = ownDesc(IntrinsicReadableStream.prototype, 'locked').get;
const readableGetReader = IntrinsicReadableStream.prototype.getReader;
const readerRead = ReadableStreamDefaultReader.prototype.read;
const readerCancel = ReadableStreamDefaultReader.prototype.cancel;
const readableCtrlProto = ReadableStreamDefaultController.prototype;
const readableCtrlEnqueue = readableCtrlProto.enqueue;
const readableCtrlClose = readableCtrlProto.close;
const readableCtrlError = readableCtrlProto.error;

const writableLockedGetter = ownDesc(IntrinsicWritableStream.prototype, 'locked').get;
const writableGetWriter = IntrinsicWritableStream.prototype.getWriter;
const writerWrite = WritableStreamDefaultWriter.prototype.write;
const writerClose = WritableStreamDefaultWriter.prototype.close;
const writerAbort = WritableStreamDefaultWriter.prototype.abort;

const requestProto = IntrinsicRequest.prototype;
const responseProto = IntrinsicResponse.prototype;
const requestBodyUsedGetter = ownDesc(requestProto, 'bodyUsed').get;
const requestHeadersGetter = ownDesc(requestProto, 'headers').get;
const requestSignalGetter = ownDesc(requestProto, 'signal').get;
const responseBodyUsedGetter = ownDesc(responseProto, 'bodyUsed').get;
const responseHeadersGetter = ownDesc(responseProto, 'headers').get;

const sharedTextEncoder = new TextEncoder();
const textEncoderEncode = TextEncoder.prototype.encode;

function isReadableStream(value) {
  if (value === null || typeof value !== 'object') {
    return false;
  }
  try {
    return typeof readableLockedGetter.call(value) === 'boolean';
  } catch {
    return false;
  }
}

function isWritableStream(value) {
  if (value === null || typeof value !== 'object') {
    return false;
  }
  try {
    return typeof writableLockedGetter.call(value) === 'boolean';
  } catch {
    return false;
  }
}

// Returns 0 = transferable, 1 = locked, 2 = disturbed/unusable.
function checkReadableTransferable(stream) {
  if (!isReadableStream(stream)) {
    return -1;
  }
  if (readableLockedGetter.call(stream)) {
    return 1;
  }
  if (ownDesc(stream, '_disturbed')?.value === true) {
    return 2;
  }
  const state = ownDesc(stream, '_state')?.value;
  if (state !== undefined && state !== 'readable') {
    return 2;
  }
  return 0;
}

// Returns 0 = transferable, 1 = locked, 2 = closed/errored.
function checkWritableTransferable(stream) {
  if (!isWritableStream(stream)) {
    return -1;
  }
  if (writableLockedGetter.call(stream)) {
    return 1;
  }
  const state = ownDesc(stream, '_state')?.value;
  if (state !== undefined && state !== 'writable') {
    return 2;
  }
  return 0;
}

// Extracts raw byte view from ArrayBuffer or ArrayBufferView without running
// user getters; returns null if `chunk` is not a binary buffer/view.
function extractBytes(chunk) {
  if (chunk === null || typeof chunk !== 'object') {
    return null;
  }
  try {
    if (arrayBufferIsView(chunk)) {
      if (getProto(chunk) === dataViewProto) {
        return new IntrinsicUint8Array(
          dataViewBuffer.call(chunk),
          dataViewByteOffset.call(chunk),
          dataViewByteLength.call(chunk),
        );
      }
      return new IntrinsicUint8Array(
        typedBuffer.call(chunk),
        typedByteOffset.call(chunk),
        typedByteLength.call(chunk),
      );
    }
    const len = arrayBufferByteLength.call(chunk);
    return new IntrinsicUint8Array(chunk, 0, len);
  } catch {
    return null;
  }
}

function attachReadableSource(stream, onChunk, onEnd, onError) {
  const reader = readableGetReader.call(stream);
  return {
    pull() {
      let readPromise;
      try {
        readPromise = readerRead.call(reader);
      } catch (err) {
        onError(err);
        return;
      }
      promiseThen.call(
        readPromise,
        (result) => {
          if (result && result.done) {
            onEnd();
          } else {
            onChunk(result ? result.value : undefined);
          }
        },
        (err) => {
          onError(err);
        },
      );
    },
    cancel(reason) {
      try {
        const p = readerCancel.call(reader, reason);
        promiseThen.call(p, () => {}, () => {});
      } catch {
        // Ignore cancel errors on already-terminated sources.
      }
    },
  };
}

function createReadableProxy(onPull, onCancel) {
  let ctrl = null;
  let closed = false;
  const stream = new IntrinsicReadableStream(
    {
      start(controller) {
        ctrl = controller;
      },
      pull() {
        return onPull();
      },
      cancel(reason) {
        closed = true;
        return onCancel(reason);
      },
    },
    { highWaterMark: 0 },
  );
  return {
    stream,
    enqueue(chunk) {
      if (closed || ctrl === null) return;
      try {
        readableCtrlEnqueue.call(ctrl, chunk);
      } catch {
        // Stream may already be canceled or errored.
      }
    },
    close() {
      if (closed || ctrl === null) return;
      closed = true;
      try {
        readableCtrlClose.call(ctrl);
      } catch {
        // Ignore double close.
      }
    },
    error(err) {
      if (closed || ctrl === null) return;
      closed = true;
      try {
        readableCtrlError.call(ctrl, err);
      } catch {
        // Ignore error after close/cancel.
      }
    },
  };
}

function attachWritableSource(stream) {
  const writer = writableGetWriter.call(stream);
  return {
    write(chunk, onDone, onError) {
      let p;
      try {
        p = writerWrite.call(writer, chunk);
      } catch (err) {
        onError(err);
        return;
      }
      promiseThen.call(p, () => onDone(), (err) => onError(err));
    },
    close(onDone, onError) {
      let p;
      try {
        p = writerClose.call(writer);
      } catch (err) {
        onError(err);
        return;
      }
      promiseThen.call(p, () => onDone(), (err) => onError(err));
    },
    abort(reason) {
      try {
        const p = writerAbort.call(writer, reason);
        promiseThen.call(p, () => {}, () => {});
      } catch {
        // Ignore abort errors on already-terminated sinks.
      }
    },
  };
}

function createWritableProxy(onWrite, onClose, onAbort) {
  let ctrl = null;
  const stream = new IntrinsicWritableStream(
    {
      start(controller) {
        ctrl = controller;
      },
      write(chunk) {
        return onWrite(chunk);
      },
      close() {
        return onClose();
      },
      abort(reason) {
        return onAbort(reason);
      },
    },
    { highWaterMark: 1 },
  );
  return {
    stream,
    error(err) {
      if (ctrl === null) return;
      try {
        ctrl.error(err);
      } catch {
        // Ignore if stream is already closed or errored.
      }
    },
  };
}

// M4C helpers (genuine Request / Response brand inspection & construction).
function isRequest(value) {
  if (value === null || typeof value !== 'object') return false;
  return (
    getProto(value) === requestProto &&
    objectHasOwn(value, '_bodyConsumed') &&
    objectHasOwn(value, '_noBody') &&
    objectHasOwn(value, 'method') &&
    objectHasOwn(value, 'url')
  );
}

function isResponse(value) {
  if (value === null || typeof value !== 'object') return false;
  return (
    getProto(value) === responseProto &&
    objectHasOwn(value, '_bodyConsumed') &&
    objectHasOwn(value, '_noBody') &&
    objectHasOwn(value, 'status')
  );
}

function inspectRequest(req) {
  if (!isRequest(req)) return null;
  if (requestBodyUsedGetter.call(req)) {
    return { error: 'Request body is already used' };
  }
  const headersObj = requestHeadersGetter.call(req);
  const headerPairs = headersObj ? bridgeHeaderPairs(headersObj) : [];
  const signal = requestSignalGetter.call(req);

  let bodyKind = 0; // 0 = none, 1 = inline bytes, 2 = stream
  let inlineBytes = null;
  let bodyStream = null;
  const fastKind = ownDesc(req, '_bodyFastKind')?.value ?? 0;
  const fastData = ownDesc(req, '_bodyFastData')?.value ?? null;
  const rawStream = ownDesc(req, '_bodyStream')?.value ?? null;

  if (fastKind === 1 && typeof fastData === 'string') {
    const encoded = textEncoderEncode.call(sharedTextEncoder, fastData);
    if (encoded.byteLength <= 65536) {
      bodyKind = 1;
      inlineBytes = encoded;
    } else {
      bodyKind = 2;
      bodyStream = req.body;
    }
  } else if (fastKind === 2 && fastData !== null) {
    const view = extractBytes(fastData);
    if (view !== null && view.byteLength <= 65536) {
      bodyKind = 1;
      inlineBytes = view;
    } else {
      bodyKind = 2;
      bodyStream = req.body;
    }
  } else if (rawStream !== null && rawStream !== undefined) {
    const check = checkReadableTransferable(rawStream);
    if (check === 1) return { error: 'Request body stream is locked' };
    if (check === 2) return { error: 'Request body stream is disturbed' };
    bodyKind = 2;
    bodyStream = rawStream;
  }

  return {
    url: String(ownDesc(req, 'url')?.value ?? ''),
    method: String(ownDesc(req, 'method')?.value ?? 'GET'),
    credentials: String(ownDesc(req, 'credentials')?.value ?? 'same-origin'),
    redirect: String(ownDesc(req, 'redirect')?.value ?? 'follow'),
    mode: ownDesc(req, 'mode')?.value ?? null,
    headers: headerPairs,
    signal: signal ?? null,
    bodyKind,
    inlineBytes,
    bodyStream,
  };
}

function createProxyRequest(meta, body, signal) {
  const headers = createHeadersFromIncomingPairs(meta.headers, false);
  const init = {
    method: meta.method,
    headers,
    credentials: meta.credentials || 'same-origin',
    redirect: meta.redirect || 'follow',
  };
  if (meta.mode !== null && meta.mode !== undefined) {
    init.mode = meta.mode;
  }
  if (signal !== null && signal !== undefined) {
    init.signal = signal;
  }
  if (body !== null && body !== undefined) {
    init.body = body;
    if (isReadableStream(body)) {
      init.duplex = 'half';
    }
  }
  return new IntrinsicRequest(meta.url, init);
}

function inspectResponse(res) {
  if (!isResponse(res)) return null;
  if (getResponseWebSocket(res) !== null) {
    return { error: 'Response with attached WebSocket cannot cross Binding RPC' };
  }
  if (responseBodyUsedGetter.call(res)) {
    return { error: 'Response body is already used' };
  }
  const headersObj = responseHeadersGetter.call(res);
  const headerPairs = headersObj ? bridgeHeaderPairs(headersObj) : [];

  let bodyKind = 0; // 0 = none, 1 = inline bytes, 2 = stream
  let inlineBytes = null;
  let bodyStream = null;
  const status = Number(ownDesc(res, 'status')?.value ?? 200);
  const fastKind = ownDesc(res, '_bodyFastKind')?.value ?? 0;
  const fastData = ownDesc(res, '_bodyFastData')?.value ?? null;
  const rawStream = ownDesc(res, '_bodyStream')?.value ?? null;

  if (!isNullFastResponseBody(res)) {
    if (fastKind === 1 && typeof fastData === 'string') {
      const encoded = textEncoderEncode.call(sharedTextEncoder, fastData);
      if (encoded.byteLength <= 65536) {
        bodyKind = 1;
        inlineBytes = encoded;
      } else {
        bodyKind = 2;
        bodyStream = res.body;
      }
    } else if (fastKind === 2 && fastData !== null) {
      const view = extractBytes(fastData);
      if (view !== null && view.byteLength <= 65536) {
        bodyKind = 1;
        inlineBytes = view;
      } else {
        bodyKind = 2;
        bodyStream = res.body;
      }
    } else {
      const stream = getFastStreamResponseBody(res) ?? rawStream;
      if (stream !== null && stream !== undefined) {
        const check = checkReadableTransferable(stream);
        if (check === 1) return { error: 'Response body stream is locked' };
        if (check === 2) return { error: 'Response body stream is disturbed' };
        bodyKind = 2;
        bodyStream = stream;
      } else {
        const fast = consumeFastResponseBody(res);
        if (fast !== null) {
          bodyKind = 1;
          inlineBytes =
            typeof fast === 'string'
              ? textEncoderEncode.call(sharedTextEncoder, fast)
              : extractBytes(fast);
        }
      }
    }
  }

  return {
    status,
    statusText: String(ownDesc(res, 'statusText')?.value ?? ''),
    url: String(ownDesc(res, 'url')?.value ?? ''),
    redirected: Boolean(ownDesc(res, 'redirected')?.value ?? false),
    type: String(ownDesc(res, 'type')?.value ?? 'default'),
    headers: headerPairs,
    bodyKind,
    inlineBytes,
    bodyStream,
  };
}

function createProxyResponse(meta, body) {
  if (meta.type === 'error' || meta.status === 0) {
    return IntrinsicResponse.error();
  }
  const headers = createHeadersFromIncomingPairs(meta.headers, false);
  const res = new IntrinsicResponse(body === undefined ? null : body, {
    status: meta.status,
    statusText: meta.statusText,
    headers,
  });
  if (meta.url) res.url = meta.url;
  if (meta.redirected) res.redirected = true;
  return res;
}

function disposeLateValue(value) {
  const abortErr = new IntrinsicDOMException('Binding call was canceled', 'AbortError');
  if (isReadableStream(value)) {
    try {
      if (!readableLockedGetter.call(value)) {
        const reader = readableGetReader.call(value);
        promiseThen.call(readerCancel.call(reader, abortErr), () => {}, () => {});
      }
    } catch {
      // Best-effort bounded disposal.
    }
    return;
  }
  if (isWritableStream(value)) {
    try {
      if (!writableLockedGetter.call(value)) {
        const writer = writableGetWriter.call(value);
        promiseThen.call(writerAbort.call(writer, abortErr), () => {}, () => {});
      }
    } catch {
      // Best-effort bounded disposal.
    }
    return;
  }
  if (isResponse(value)) {
    const stream = ownDesc(value, '_bodyStream')?.value ?? null;
    if (stream !== null && isReadableStream(stream)) {
      disposeLateValue(stream);
    }
    return;
  }
  // M4D RpcTarget or Symbol.dispose resource cleanup (bounded, fault-isolated).
  if (value !== null && typeof value === 'object') {
    const symDispose = Symbol.dispose;
    if (symDispose !== undefined) {
      const fn = value[symDispose];
      if (typeof fn === 'function') {
        try {
          const ret = fn.call(value);
          if (ret !== null && typeof ret === 'object' && typeof ret.then === 'function') {
            promiseThen.call(promiseResolve(ret), () => {}, () => {});
          }
        } catch {
          // Fault-isolated disposal.
        }
      }
    }
  }
}

export function installBindingRpcBridge(core) {
  if (typeof core.capsidInstallBindingBridge !== 'function') {
    return;
  }
  core.capsidInstallBindingBridge({
    isReadableStream,
    isWritableStream,
    checkReadableTransferable,
    checkWritableTransferable,
    extractBytes,
    attachReadableSource,
    createReadableProxy,
    attachWritableSource,
    createWritableProxy,
    isRequest,
    isResponse,
    inspectRequest,
    createProxyRequest,
    inspectResponse,
    createProxyResponse,
    disposeLateValue,
  });
}
