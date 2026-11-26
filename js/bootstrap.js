// Import order follows the dependencies between Web Platform APIs.
import 'txiki-polyfills/self.js';
import 'txiki-polyfills/timers.js';
import 'txiki-polyfills/event-target-polyfill.js';
import { QuotaExceededError, installStructuredCloneDomExceptionSupport } from './dom-exception.js';
import { ErrorEvent } from './events.js';
import { dispatchTrustedEvent } from 'txiki-polyfills/event-target.js';
import 'txiki-polyfills/structured-clone.js';
installStructuredCloneDomExceptionSupport();
import './message-channel.js';
import 'txiki-polyfills/text-encoding.js';
import 'txiki-polyfills/text-encode-transform.js';
import './text-decoder.js';
import 'txiki-polyfills/url.js';
import './url-search-params.js';
import './url.js';
import 'txiki-polyfills/blob.js';
import 'txiki-polyfills/file.js';
import './file.js';
import 'txiki-polyfills/form-data.js';
import 'txiki-polyfills/abort-controller.js';
import 'txiki-polyfills/fetch/polyfill.js';
import {
  configureFetchLimits,
  createIncomingRequest,
  bridgeResponseHeaders,
  bridgeResponseHeadersFlat,
  clearBridgeFlatScratch,
  bridgeResponseHadContentLength,
  consumeFastResponseBody,
  getFastStreamResponseBody,
  isNullFastResponseBody,
} from './fetch.js';
import 'txiki-polyfills/crypto/crypto.js';
import { CryptoKey } from 'txiki-polyfills/crypto/crypto-key.js';
import { SubtleCrypto } from 'txiki-polyfills/crypto/subtle.js';
import './streams.js';
import 'txiki-polyfills/compression-streams.js';
import 'txiki-polyfills/wasm.js';
import './wasm.js';

import core from 'tjs:internal/core';
import { transferWebSocket, isFetchChannelStream, getNativeDefaultStreamOps } from './websocket.js';
import { profileGlobalNames } from './profile-manifest.js';

configureFetchLimits(core.capsidFetchRequestBodyLimit(), core.capsidFetchResponseBodyLimit());
const readableStreamProto = ReadableStream.prototype;
const wrappedStreamGetReader = readableStreamProto.getReader;
const defaultReaderProto = ReadableStreamDefaultReader.prototype;
const wrappedReaderRead = defaultReaderProto.read;
const { nativeGetReader, nativeRead } = getNativeDefaultStreamOps();
const intrinsicCall = Function.prototype.call.bind(Function.prototype.call);
const fixedResponseBodyLimit = Number(core.capsidFixedResponseBodyLimit());

const nativeCrypto = globalThis.crypto;
const TypedArrayPrototype = Object.getPrototypeOf(Uint8Array.prototype);
const typedArrayToStringTag = Object.getOwnPropertyDescriptor(
  TypedArrayPrototype,
  Symbol.toStringTag,
).get;
const typedArrayBuffer = Object.getOwnPropertyDescriptor(TypedArrayPrototype, 'buffer').get;
const dataViewBuffer = Object.getOwnPropertyDescriptor(DataView.prototype, 'buffer').get;
const typedArrayByteOffset = Object.getOwnPropertyDescriptor(TypedArrayPrototype, 'byteOffset').get;
const typedArrayByteLength = Object.getOwnPropertyDescriptor(TypedArrayPrototype, 'byteLength').get;
const arrayBufferIsView = ArrayBuffer.isView;
const integerTypedArrayTags = new Set([
  'Int8Array',
  'Uint8Array',
  'Uint8ClampedArray',
  'Int16Array',
  'Uint16Array',
  'Int32Array',
  'Uint32Array',
  'BigInt64Array',
  'BigUint64Array',
]);
let cryptoObject;
class Crypto {
  constructor() {
    throw new TypeError('Illegal constructor');
  }

  get subtle() {
    if (this !== cryptoObject) {
      throw new TypeError('Illegal invocation');
    }
    return nativeCrypto.subtle;
  }

  getRandomValues(value) {
    if (this !== cryptoObject) {
      throw new TypeError('Illegal invocation');
    }
    if (!arrayBufferIsView(value)) {
      throw new TypeError(
        "Failed to execute 'getRandomValues' on 'Crypto': parameter 1 is not of type 'ArrayBufferView'.",
      );
    }
    const tag = typedArrayToStringTag.call(value);
    const buffer = tag === undefined ? dataViewBuffer.call(value) : typedArrayBuffer.call(value);
    if (!core.isArrayBuffer(buffer)) {
      throw new TypeError('getRandomValues requires a non-shared buffer');
    }
    if (!integerTypedArrayTags.has(tag)) {
      throw new DOMException(
        'The provided ArrayBufferView is not an integer typed array',
        'TypeMismatchError',
      );
    }
    const byteLength = typedArrayByteLength.call(value);
    if (byteLength > 65536) {
      throw new QuotaExceededError('The requested length exceeds 65536 bytes');
    }
    if (byteLength !== 0) {
      core.random(buffer, typedArrayByteOffset.call(value), byteLength);
    }
    return value;
  }

  randomUUID() {
    if (this !== cryptoObject) {
      throw new TypeError('Illegal invocation');
    }
    return nativeCrypto.randomUUID();
  }
}
cryptoObject = Object.freeze(Object.create(Crypto.prototype));
Object.defineProperties(globalThis, {
  Crypto: {
    configurable: true,
    writable: true,
    value: Crypto,
  },
  CryptoKey: {
    configurable: true,
    writable: true,
    value: CryptoKey,
  },
  SubtleCrypto: {
    configurable: true,
    writable: true,
    value: SubtleCrypto,
  },
  crypto: {
    configurable: true,
    enumerable: true,
    writable: true,
    value: cryptoObject,
  },
});

const nativePerformance = globalThis.performance;
let performanceObject;
class Performance {
  constructor() {
    throw new TypeError('Illegal constructor');
  }

  get timeOrigin() {
    if (this !== performanceObject) {
      throw new TypeError('Illegal invocation');
    }
    return nativePerformance.timeOrigin;
  }

  now() {
    if (this !== performanceObject) {
      throw new TypeError('Illegal invocation');
    }
    return nativePerformance.now();
  }

  toJSON() {
    return {
      timeOrigin: this.timeOrigin,
    };
  }
}
Object.setPrototypeOf(Performance.prototype, EventTarget.prototype);
performanceObject = Object.freeze(Object.create(Performance.prototype));
EventTarget.prototype.__init.call(performanceObject);
Object.defineProperties(globalThis, {
  Performance: {
    configurable: true,
    writable: true,
    value: Performance,
  },
  performance: {
    configurable: true,
    enumerable: true,
    writable: true,
    value: performanceObject,
  },
});

const formatLogValue = (value) => {
  if (typeof value === 'string') {
    return value;
  }
  try {
    return JSON.stringify(value);
  } catch {
    return String(value);
  }
};

const consolePrototype = Object.create(Object.prototype);
const consoleObject = Object.create(consolePrototype);
const consoleCounts = new Map();
const consoleTimers = new Map();
let consoleGroupDepth = 0;

const emitConsole = (level, values) => {
  const prefix = '  '.repeat(consoleGroupDepth);
  core.capsidLog(level, prefix + values.map(formatLogValue).join(' '));
};

for (const level of ['debug', 'error', 'info', 'log', 'warn']) {
  Object.defineProperty(consoleObject, level, {
    enumerable: true,
    configurable: false,
    writable: false,
    value(...values) {
      emitConsole(level, values);
    },
  });
}

const defineConsoleMethod = (name, implementation) => {
  Object.defineProperty(consoleObject, name, {
    enumerable: true,
    configurable: false,
    writable: false,
    value: implementation,
  });
};

defineConsoleMethod('assert', (condition = false, ...values) => {
  if (!condition) {
    const prefix =
      values.length > 0 && typeof values[0] === 'string'
        ? `Assertion failed: ${values.shift()}`
        : 'Assertion failed';
    emitConsole('error', [prefix, ...values]);
  }
});
defineConsoleMethod('clear', () => {
  consoleGroupDepth = 0;
  emitConsole('clear', []);
});
defineConsoleMethod('count', (label = 'default') => {
  label = String(label);
  const count = (consoleCounts.get(label) ?? 0) + 1;
  consoleCounts.set(label, count);
  emitConsole('log', [`${label}: ${count}`]);
});
defineConsoleMethod('countReset', (label = 'default') => {
  label = String(label);
  if (!consoleCounts.delete(label)) {
    emitConsole('warn', [`countReset: No counter named ${label}`]);
  }
});
defineConsoleMethod('dir', (item, _options) => {
  emitConsole('log', [item]);
});
defineConsoleMethod('dirxml', (...values) => {
  emitConsole('log', values);
});
defineConsoleMethod('group', (...values) => {
  if (values.length > 0) {
    emitConsole('log', values);
  }
  consoleGroupDepth += 1;
});
defineConsoleMethod('groupCollapsed', (...values) => {
  consoleObject.group(...values);
});
defineConsoleMethod('groupEnd', () => {
  consoleGroupDepth = Math.max(0, consoleGroupDepth - 1);
});
defineConsoleMethod('table', (data, properties) => {
  if (properties !== undefined && !Array.isArray(properties)) {
    throw new TypeError('console.table properties must be an array');
  }
  emitConsole('log', [data]);
});
defineConsoleMethod('time', (label = 'default') => {
  label = String(label);
  if (consoleTimers.has(label)) {
    emitConsole('warn', [`Timer ${label} already exists`]);
    return;
  }
  consoleTimers.set(label, performance.now());
});
defineConsoleMethod('timeLog', (label = 'default', ...values) => {
  label = String(label);
  const start = consoleTimers.get(label);
  if (start === undefined) {
    emitConsole('warn', [`timeLog: No such timer: ${label}`]);
    return;
  }
  emitConsole('log', [`${label}: ${performance.now() - start}ms`, ...values]);
});
defineConsoleMethod('timeEnd', (label = 'default') => {
  label = String(label);
  const start = consoleTimers.get(label);
  if (start === undefined) {
    emitConsole('warn', [`timeEnd: No such timer: ${label}`]);
    return;
  }
  consoleTimers.delete(label);
  emitConsole('log', [`${label}: ${performance.now() - start}ms`]);
});
defineConsoleMethod('trace', (...values) => {
  const stack = new Error().stack;
  emitConsole('error', [`Trace: ${values.map(formatLogValue).join(' ')}`, stack]);
});

Object.defineProperty(globalThis, 'console', {
  enumerable: false,
  configurable: true,
  writable: true,
  value: Object.freeze(consoleObject),
});

Object.defineProperty(globalThis, 'navigator', {
  enumerable: true,
  configurable: false,
  writable: false,
  value: Object.freeze({ userAgent: 'capsid-runtime' }),
});

Object.defineProperty(globalThis, 'reportError', {
  enumerable: true,
  configurable: true,
  writable: true,
  value(error) {
    if (arguments.length === 0) {
      throw new TypeError('reportError requires an argument');
    }
    const stack = new Error().stack ?? '';
    let filename = '';
    let lineno = 0;
    let colno = 0;
    for (const line of stack.split('\n')) {
      if (line.includes('tjs:internal/capsid-bootstrap')) {
        continue;
      }
      const match = /\((.+):(\d+):(\d+)\)$/.exec(line) ?? /^\s*at (.+):(\d+):(\d+)$/.exec(line);
      if (match) {
        filename = match[1];
        lineno = Number(match[2]);
        colno = Number(match[3]);
        break;
      }
    }

    let message;
    if (error instanceof Error) {
      const ownMessage = Object.getOwnPropertyDescriptor(error, 'message')?.value;
      const constructor = Object.getOwnPropertyDescriptor(
        Object.getPrototypeOf(error),
        'constructor',
      )?.value;
      const errorName =
        Object.getOwnPropertyDescriptor(constructor ?? {}, 'name')?.value || 'Error';
      message =
        `Uncaught ${errorName}` +
        (typeof ownMessage === 'string' && ownMessage ? `: ${ownMessage}` : '');
    } else {
      message = `Uncaught ${String(error)}`;
    }
    const event = new ErrorEvent('error', {
      cancelable: true,
      error,
      message,
      filename,
      lineno,
      colno,
    });
    if (dispatchTrustedEvent(globalThis, event)) {
      consoleObject.error(message);
    }
  },
});

core.defineEventAttribute(Object.getPrototypeOf(globalThis), 'error');

// Runtime microtask errors use captured intrinsics and native source metadata.
// Event identity and origin survive writes to public reportError and stack.
const microtaskErrorCtor = Error;
const microtaskString = String;
const microtaskOwnDescriptor = Object.getOwnPropertyDescriptor;
const microtaskPrototype = Object.getPrototypeOf;
core.capsidInstallMicrotaskReporter((error, location) => {
  try {
    let message;
    if (error instanceof microtaskErrorCtor) {
      const ownMessage = microtaskOwnDescriptor(error, 'message')?.value;
      const constructor = microtaskOwnDescriptor(microtaskPrototype(error), 'constructor')?.value;
      const name = microtaskOwnDescriptor(constructor ?? {}, 'name')?.value || 'Error';
      message =
        `Uncaught ${name}` +
        (typeof ownMessage === 'string' && ownMessage ? `: ${ownMessage}` : '');
    } else message = `Uncaught ${microtaskString(error)}`;
    const event = new ErrorEvent('error', {
      error,
      message,
      filename: location[0] === undefined ? 'undefined' : location[0],
      lineno: location[1],
      colno: location[2],
    });
    if (dispatchTrustedEvent(globalThis, event)) consoleObject.error(message);
  } catch (secondary) {
    consoleObject.error(secondary);
  }
});

for (const name of ['ProgressEvent']) {
  delete globalThis[name];
}

const requests = new Map();

const formatRuntimeError = (error) => {
  let summary = '';
  let stack = '';
  try {
    summary = String(error);
  } catch {
    summary = 'Error';
  }
  try {
    stack = typeof error?.stack === 'string' ? error.stack : '';
  } catch {
    // A hostile stack getter must not hide the original failure.
  }
  if (!stack) {
    return summary;
  }
  return summary && !stack.includes(summary) ? `${summary}\n${stack}` : stack;
};

function pullRequestBody(state) {
  if (state.chunks.length > 0) {
    const chunk = state.chunks.shift();
    const byteLength = chunk.byteLength;
    state.waiting = false;
    state.controller.enqueue(chunk);
    core.capsidRequestCredit(state.id, byteLength);
  } else if (state.ended) {
    state.waiting = false;
    state.controller.close();
  } else {
    state.waiting = true;
  }
}

const IntrinsicPromise = Promise;
const intrinsicPromisePrototype = IntrinsicPromise.prototype;
const intrinsicPromiseThen = intrinsicPromisePrototype.then;
const intrinsicPromiseResolve = Promise.resolve;
const enqueueMicrotask = globalThis.queueMicrotask;
const intrinsicApply = Reflect.apply;
const funcProto = Function.prototype;
const funcCall = funcProto.call;
const responsePrototype = Response.prototype;
const getObjectPrototype = Object.getPrototypeOf;
const objectHasOwn = Object.hasOwn;

function promiseThen(promise, onFulfilled, onRejected) {
  if (funcProto.call === funcCall) {
    return intrinsicPromiseThen.call(promise, onFulfilled, onRejected);
  }
  return intrinsicApply(intrinsicPromiseThen, promise, [onFulfilled, onRejected]);
}

function markRequestSettledJS(state) {
  if (state.active) {
    state.active = false;
    requests.delete(state.id);
  }
}

function settleRequest(state) {
  if (state.active) {
    state.active = false;
    requests.delete(state.id);
    // WP-02 §6.4: settled signal — the last lifecycle bridge.
    // Native marks the token terminal; the post-drain reclaim
    // drops it (or poisons the worker on surviving refs).
    core.capsidRequestSettled(state.id);
  }
}

function failRequest(state, error) {
  if (!state.cancelled) {
    core.capsidResponseError(state.id, formatRuntimeError(error));
  }
  settleRequest(state);
}

async function finishResponseAsync(state, response) {
  const id = state.id;
  if (state.cancelled) {
    if (response instanceof Response && response.body) {
      try {
        await response.body.cancel(state.cancelReason);
      } catch {
        // Cancellation cleanup must not revive the request.
      }
    }
    return;
  }
  if (!(response instanceof Response)) {
    throw new TypeError('application fetch handler must return a Response');
  }

  if (response.status === 101) {
    const upgradeHeaders = bridgeResponseHeaders(response);
    try {
      await transferWebSocket(id, response, upgradeHeaders);
    } catch (error) {
      // Origin/negotiation failures precede 101. The transfer
      // path owns post-commit coupling errors and aborts their
      // transport; it must never append another HTTP response.
      core.capsidResponseFinal(
        id,
        500,
        'WebSocket transfer failed',
        [],
        String(error?.message ?? error),
      );
    }
    return;
  }

  const headers = bridgeResponseHeadersFlat(response);
  const hasContentLength = bridgeResponseHadContentLength();
  const body = response.body;
  if (body && !(body instanceof ReadableStream)) {
    let bytes = null;
    if (body instanceof Uint8Array) {
      bytes = body;
    } else if (typeof body === 'string') {
      bytes = body;
    } else if (body instanceof ArrayBuffer) {
      bytes = new Uint8Array(body);
    } else if (body instanceof Blob) {
      bytes = new Uint8Array(await body.arrayBuffer());
    } else {
      bytes = null;
    }
    if (bytes !== null) {
      const fixedCandidate =
        !hasContentLength &&
        (typeof bytes === 'string'
          ? bytes.length <= fixedResponseBodyLimit
          : bytes.byteLength <= fixedResponseBodyLimit);
      try {
        if (fixedCandidate) {
          core.capsidResponseFixed(id, response.status, response.statusText, headers, bytes);
        } else {
          core.capsidResponseFinal(id, response.status, response.statusText, headers, bytes);
        }
      } finally {
        clearBridgeFlatScratch();
      }
      return;
    }
  }

  try {
    core.capsidResponseHead(id, response.status, response.statusText, headers);
  } finally {
    clearBridgeFlatScratch();
  }

  if (body) {
    const reader = body.getReader();
    state.responseReader = reader;
    try {
      while (true) {
        const result = await reader.read();
        if (result.done || state.cancelled) {
          break;
        }
        // E13a: unblocked writes complete synchronously and
        // return undefined; only credit-blocked writes
        // return a promise to await.
        const pending = core.capsidResponseWrite(id, result.value);
        if (pending !== undefined) {
          await pending;
        }
      }
    } finally {
      if (state.responseReader === reader) {
        state.responseReader = null;
      }
    }
  }
  if (!state.cancelled) {
    core.capsidResponseEnd(id);
  }
}

function pumpStreamResponse(state, response, body) {
  const id = state.id;
  const headers = bridgeResponseHeadersFlat(response);
  try {
    core.capsidResponseHead(id, response.status, response.statusText, headers);
  } finally {
    clearBridgeFlatScratch();
  }
  const useNativeReader =
    nativeGetReader !== null &&
    !isFetchChannelStream(body) &&
    getObjectPrototype(body) === readableStreamProto &&
    !objectHasOwn(body, 'getReader') &&
    readableStreamProto.getReader === wrappedStreamGetReader &&
    defaultReaderProto.read === wrappedReaderRead;
  const reader = useNativeReader ? intrinsicCall(nativeGetReader, body) : body.getReader();
  state.responseReader = reader;

  const onError = (error) => {
    if (state.responseReader === reader) {
      state.responseReader = null;
    }
    failRequest(state, error);
  };

  const finishStream = () => {
    if (state.responseReader === reader) {
      state.responseReader = null;
    }
    if (!state.cancelled) {
      try {
        core.capsidResponseEnd(id, true);
      } catch (error) {
        failRequest(state, error);
        return;
      }
      markRequestSettledJS(state);
      return;
    }
    settleRequest(state);
  };

  const onChunk = (result) => {
    if (result.done || state.cancelled) {
      finishStream();
      return;
    }
    let pending;
    try {
      pending = core.capsidResponseWrite(id, result.value);
    } catch (error) {
      onError(error);
      return;
    }
    if (pending !== undefined) {
      promiseThen(pending, drainReads, onError);
      return;
    }
    drainReads();
  };

  const drainReads = () => {
    while (true) {
      if (state.cancelled) {
        if (state.responseReader === reader) {
          state.responseReader = null;
        }
        settleRequest(state);
        return;
      }
      let readPromise;
      try {
        readPromise = useNativeReader ? intrinsicCall(nativeRead, reader) : reader.read();
      } catch (error) {
        onError(error);
        return;
      }
      if (
        readPromise !== null &&
        typeof readPromise === 'object' &&
        intrinsicPromisePrototype.then === intrinsicPromiseThen &&
        getObjectPrototype(readPromise) === intrinsicPromisePrototype &&
        !objectHasOwn(readPromise, 'then')
      ) {
        const syncResult = core.capsidTryTakeFulfilledPromise(readPromise);
        if (syncResult !== null) {
          if (syncResult.done || state.cancelled) {
            finishStream();
            return;
          }
          let pending;
          try {
            pending = core.capsidResponseWrite(id, syncResult.value);
          } catch (error) {
            onError(error);
            return;
          }
          if (pending !== undefined) {
            promiseThen(pending, drainReads, onError);
            return;
          }
          continue;
        }
      }
      promiseThen(readPromise, onChunk, onError);
      return;
    }
  };

  drainReads();
}

function completeResponse(state, response) {
  if (!state.cancelled) {
    try {
      if (
        response === null ||
        typeof response !== 'object' ||
        (getObjectPrototype(response) !== responsePrototype && !(response instanceof Response))
      ) {
        failRequest(state, new TypeError('application fetch handler must return a Response'));
        return;
      }
      if (response.status !== 101) {
        const fastBody = consumeFastResponseBody(response);
        if (fastBody !== null) {
          const headers = bridgeResponseHeadersFlat(response);
          const length = typeof fastBody === 'string' ? fastBody.length : fastBody.byteLength;
          try {
            if (length <= fixedResponseBodyLimit && !bridgeResponseHadContentLength()) {
              core.capsidResponseFixed(
                state.id,
                response.status,
                response.statusText,
                headers,
                fastBody,
                true,
              );
            } else {
              core.capsidResponseFinal(
                state.id,
                response.status,
                response.statusText,
                headers,
                fastBody,
                true,
              );
            }
          } finally {
            clearBridgeFlatScratch();
          }
          markRequestSettledJS(state);
          return;
        }
        const streamBody = getFastStreamResponseBody(response);
        if (streamBody !== null) {
          pumpStreamResponse(state, response, streamBody);
          return;
        }
        if (isNullFastResponseBody(response)) {
          const headers = bridgeResponseHeadersFlat(response);
          try {
            core.capsidResponseHead(state.id, response.status, response.statusText, headers);
          } finally {
            clearBridgeFlatScratch();
          }
          core.capsidResponseEnd(state.id, true);
          markRequestSettledJS(state);
          return;
        }
      }
    } catch (error) {
      failRequest(state, error);
      return;
    }
  }

  const pending = finishResponseAsync(state, response);
  promiseThen(
    pending,
    () => settleRequest(state),
    (error) => failRequest(state, error),
  );
}

const beginRequest = (handler, handlerThis, id, method, url, headerEntries, flags = 0) => {
  if (requests.size !== 0 && requests.has(id)) {
    throw new TypeError(`duplicate request id ${id}`);
  }

  const hasBody = method !== 'GET' && method !== 'HEAD';
  const bodyless = (flags & 1) !== 0;
  const canonicalHeaders = (flags & 2) !== 0;
  const state = {
    id,
    active: true,
    abortController: null,
    chunks: hasBody ? [] : null,
    controller: null,
    ended: !hasBody || bodyless,
    waiting: false,
    cancelled: false,
    cancelReason: null,
    responseReader: null,
  };
  requests.set(id, state);

  let body = null;
  if (hasBody) {
    body = new ReadableStream({
      type: 'bytes',
      start(controller) {
        state.controller = controller;
      },
      pull() {
        pullRequestBody(state);
      },
      cancel() {
        state.ended = true;
        state.chunks.length = 0;
      },
    });
  }

  const request = createIncomingRequest(
    url,
    method,
    headerEntries,
    body,
    hasBody,
    state,
    canonicalHeaders,
  );

  // WP-02 §6.2: request identity rides the job-context token captured at
  // enqueue; no JS-side enter/leave is needed (or trusted) anymore.
  enqueueMicrotask(() => {
    let result;
    try {
      result = handler.call(handlerThis, request);
      if (result !== null && typeof result === 'object') {
        if (getObjectPrototype(result) === responsePrototype && result.then === undefined) {
          completeResponse(state, result);
          return;
        }
        if (result instanceof IntrinsicPromise) {
          promiseThen(
            result,
            (response) => completeResponse(state, response),
            (error) => failRequest(state, error),
          );
          return;
        }
        if (result instanceof Response && typeof result.then !== 'function') {
          completeResponse(state, result);
          return;
        }
      }
      if (result !== null && (typeof result === 'object' || typeof result === 'function')) {
        const thenMethod = result.then;
        if (typeof thenMethod === 'function') {
          promiseThen(
            intrinsicApply(intrinsicPromiseResolve, IntrinsicPromise, [result]),
            (response) => completeResponse(state, response),
            (error) => failRequest(state, error),
          );
          return;
        }
      }
    } catch (error) {
      failRequest(state, error);
      return;
    }
    completeResponse(state, result);
  });
};

const requestChunk = (id, chunk) => {
  const state = requests.get(id);
  if (!state || state.ended || !state.controller) {
    throw new TypeError(`request ${id} does not accept body data`);
  }
  if (state.waiting) {
    const byteLength = chunk.byteLength;
    state.waiting = false;
    state.controller.enqueue(chunk);
    core.capsidRequestCredit(id, byteLength);
  } else {
    state.chunks.push(chunk);
  }
};

const requestEnd = (id) => {
  const state = requests.get(id);
  if (!state || state.ended) {
    return;
  }
  state.ended = true;
  if (state.waiting && state.chunks.length === 0) {
    state.waiting = false;
    state.controller.close();
  }
};

const cancelRequest = (id) => {
  const state = requests.get(id);
  if (!state) {
    return;
  }
  state.active = false;
  requests.delete(id);
  state.cancelled = true;
  state.ended = true;
  if (state.chunks !== null) {
    state.chunks.length = 0;
  }
  const reason = new DOMException('Request canceled', 'AbortError');
  state.cancelReason = reason;
  if (state.abortController !== null) {
    state.abortController.abort(reason);
  }
  if (state.responseReader) {
    const reader = state.responseReader;
    state.responseReader = null;
    intrinsicApply(
      intrinsicPromiseThen,
      intrinsicApply(intrinsicPromiseResolve, IntrinsicPromise, [reader.cancel(reason)]),
      [undefined, () => {}],
    );
  }
  if (state.controller) {
    try {
      state.controller.error(reason);
    } catch {
      // The stream may already be closed by the application.
    }
  }
};

core.capsidInstallBridge(beginRequest, requestChunk, requestEnd, cancelRequest);

const allowedGlobals = new Set(profileGlobalNames);
for (const name of Object.getOwnPropertyNames(globalThis)) {
  if (allowedGlobals.has(name)) {
    continue;
  }
  const descriptor = Object.getOwnPropertyDescriptor(globalThis, name);
  if (!descriptor?.configurable || !delete globalThis[name]) {
    throw new TypeError(`unexpected non-removable global: ${name}`);
  }
}
for (const name of profileGlobalNames) {
  if (!Object.prototype.hasOwnProperty.call(globalThis, name)) {
    throw new TypeError(`required profile global is missing: ${name}`);
  }
}
