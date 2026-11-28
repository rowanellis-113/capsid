// Binding RPC v2 (v0.4.0): tamper-proof internal JS bridge for stream proxies,
// controlled Request/Response transfer, and late-result resource disposal.
//
// Evaluated once during worker bootstrap BEFORE any application or Binding
// package code runs, capturing intrinsic constructors, prototypes, and brand-
// checked accessors privately. Registered via the one-shot core hook
// `capsidInstallBindingBridge` and never exposed on globalThis.

import {
  createHeadersFromIncomingPairs,
  markResponseHeadersImmutable,
  consumeFastResponseBody,
  getFastStreamResponseBody,
  isNullFastResponseBody,
  isGenuineRequest,
  isGenuineResponse,
  tryBridgeOwnedHeaderPairs,
} from './fetch.js';
import { getResponseWebSocket } from './websocket.js';

const ownDesc = Object.getOwnPropertyDescriptor;
const getProto = Object.getPrototypeOf;
const objectDefineProperty = Object.defineProperty;
const objectGetOwnPropertyNames = Object.getOwnPropertyNames;
const ObjectProto = Object.prototype;
const arrayIsArray = Array.isArray;
const numberIsInteger = Number.isInteger;
const reflectApply = Reflect.apply;
const reflectConstruct = Reflect.construct;
const IntrinsicProxy = Proxy;
const IntrinsicPromise = Promise;
const promiseThen = IntrinsicPromise.prototype.then;
const promiseResolve = IntrinsicPromise.resolve.bind(IntrinsicPromise);
const promiseReject = IntrinsicPromise.reject.bind(IntrinsicPromise);
const IntrinsicSet = Set;
const setHas = IntrinsicSet.prototype.has;
const setAdd = IntrinsicSet.prototype.add;
const IntrinsicWeakMap = WeakMap;
const weakMapGet = IntrinsicWeakMap.prototype.get;
const weakMapSet = IntrinsicWeakMap.prototype.set;

const IntrinsicReadableStream = ReadableStream;
const IntrinsicWritableStream = WritableStream;
const IntrinsicReadableStreamDefaultReader = ReadableStreamDefaultReader;
const IntrinsicWritableStreamDefaultWriter = WritableStreamDefaultWriter;
const IntrinsicRequest = Request;
const IntrinsicResponse = Response;
const IntrinsicAbortController = AbortController;
const abortControllerAbort = IntrinsicAbortController.prototype.abort;
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
const readableCancel = IntrinsicReadableStream.prototype.cancel;
const readableGetReader = IntrinsicReadableStream.prototype.getReader;
const readableTee = IntrinsicReadableStream.prototype.tee;
const readablePipeTo = IntrinsicReadableStream.prototype.pipeTo;
const readablePipeThrough = IntrinsicReadableStream.prototype.pipeThrough;
const readableValues = IntrinsicReadableStream.prototype.values;
const readableAsyncIterator = IntrinsicReadableStream.prototype[Symbol.asyncIterator];
const readerRead = IntrinsicReadableStreamDefaultReader.prototype.read;
const readerCancel = IntrinsicReadableStreamDefaultReader.prototype.cancel;
const readerReleaseLock = IntrinsicReadableStreamDefaultReader.prototype.releaseLock;
const readableCtrlProto = ReadableStreamDefaultController.prototype;
const readableCtrlEnqueue = readableCtrlProto.enqueue;
const readableCtrlClose = readableCtrlProto.close;
const readableCtrlError = readableCtrlProto.error;

const writableLockedGetter = ownDesc(IntrinsicWritableStream.prototype, 'locked').get;
const writableAbort = IntrinsicWritableStream.prototype.abort;
const writableClose = IntrinsicWritableStream.prototype.close;
const writableGetWriter = IntrinsicWritableStream.prototype.getWriter;
const writerWrite = IntrinsicWritableStreamDefaultWriter.prototype.write;
const writerClose = IntrinsicWritableStreamDefaultWriter.prototype.close;
const writerAbort = IntrinsicWritableStreamDefaultWriter.prototype.abort;
const writerReleaseLock = IntrinsicWritableStreamDefaultWriter.prototype.releaseLock;

const requestProto = IntrinsicRequest.prototype;
const responseProto = IntrinsicResponse.prototype;
const requestBodyGetter = ownDesc(requestProto, 'body').get;
const requestHeadersGetter = ownDesc(requestProto, 'headers').get;
const requestSignalGetter = ownDesc(requestProto, 'signal').get;
const responseBodyGetter = ownDesc(responseProto, 'body').get;
const responseHeadersGetter = ownDesc(responseProto, 'headers').get;

const sharedTextEncoder = new TextEncoder();
const textEncoderEncode = TextEncoder.prototype.encode;

// F4: Private ownership-fence registries for User-side Binding stream proxies.
// Checked BEFORE js/streams.js mutates stream/reader/writer/controller state so
// a rejected cross-request call can never close, lock, disturb, or abort the
// owning request's stream.
const fencedReadableStreams = new IntrinsicWeakMap();
const fencedReadableReaders = new IntrinsicWeakMap();
const fencedWritableStreams = new IntrinsicWeakMap();
const fencedWritableWriters = new IntrinsicWeakMap();
let streamFenceGuardsInstalled = false;

function makeStreamFenceError() {
  return new IntrinsicDOMException(
    'Cannot use Binding stream proxy outside its owning request',
    'InvalidStateError',
  );
}

function isObjectOrFunc(value) {
  return value !== null && (typeof value === 'object' || typeof value === 'function');
}

function checkStreamFence(map, target) {
  if (!isObjectOrFunc(target)) {
    return true;
  }
  const checkOwner = weakMapGet.call(map, target);
  return checkOwner === undefined || Boolean(checkOwner());
}

function getStreamFence(map, target) {
  if (!isObjectOrFunc(target)) {
    return undefined;
  }
  return weakMapGet.call(map, target);
}

function defineGuardedMethod(proto, key, wrapper) {
  const desc = ownDesc(proto, key);
  if (!desc || typeof desc.value !== 'function') {
    return;
  }
  const origName = ownDesc(desc.value, 'name');
  if (origName && typeof origName.value === 'string') {
    objectDefineProperty(wrapper, 'name', { value: origName.value, configurable: true });
  }
  objectDefineProperty(proto, key, {
    value: wrapper,
    writable: desc.writable,
    enumerable: desc.enumerable,
    configurable: desc.configurable,
  });
}

function wrapFencedAsyncIterator(iterator, checkOwner) {
  if (!isObjectOrFunc(iterator)) {
    return iterator;
  }
  const nextFn = iterator.next;
  if (typeof nextFn === 'function') {
    objectDefineProperty(iterator, 'next', {
      value: function next() {
        if (!checkOwner()) {
          return promiseReject(makeStreamFenceError());
        }
        return reflectApply(nextFn, this, arguments);
      },
      writable: true,
      configurable: true,
    });
  }
  const returnFn = iterator.return;
  if (typeof returnFn === 'function') {
    objectDefineProperty(iterator, 'return', {
      value: function () {
        if (!checkOwner()) {
          return promiseReject(makeStreamFenceError());
        }
        return reflectApply(returnFn, this, arguments);
      },
      writable: true,
      configurable: true,
    });
  }
  return iterator;
}

// Installed during User bootstrap ONLY when the worker loads at least one
// Binding (never on zero-Binding workers or Binding runtimes). Runs before any
// application module evaluates so saved prototype references are also guarded.
function installStreamFenceGuards() {
  if (streamFenceGuardsInstalled) {
    return;
  }
  streamFenceGuardsInstalled = true;

  defineGuardedMethod(IntrinsicReadableStream.prototype, 'cancel', function cancel() {
    if (!checkStreamFence(fencedReadableStreams, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(readableCancel, this, arguments);
  });

  defineGuardedMethod(IntrinsicReadableStream.prototype, 'getReader', function getReader() {
    const checkOwner = getStreamFence(fencedReadableStreams, this);
    if (checkOwner !== undefined && !checkOwner()) {
      throw makeStreamFenceError();
    }
    const reader = reflectApply(readableGetReader, this, arguments);
    if (checkOwner !== undefined && isObjectOrFunc(reader)) {
      weakMapSet.call(fencedReadableReaders, reader, checkOwner);
    }
    return reader;
  });

  defineGuardedMethod(IntrinsicReadableStream.prototype, 'tee', function tee() {
    const checkOwner = getStreamFence(fencedReadableStreams, this);
    if (checkOwner !== undefined && !checkOwner()) {
      throw makeStreamFenceError();
    }
    const branches = reflectApply(readableTee, this, arguments);
    if (checkOwner !== undefined && arrayIsArray(branches)) {
      for (let i = 0; i < branches.length; i++) {
        if (isObjectOrFunc(branches[i])) {
          weakMapSet.call(fencedReadableStreams, branches[i], checkOwner);
        }
      }
    }
    return branches;
  });

  defineGuardedMethod(IntrinsicReadableStream.prototype, 'pipeTo', function pipeTo(dest) {
    if (
      !checkStreamFence(fencedReadableStreams, this) ||
      !checkStreamFence(fencedWritableStreams, dest)
    ) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(readablePipeTo, this, arguments);
  });

  defineGuardedMethod(
    IntrinsicReadableStream.prototype,
    'pipeThrough',
    function pipeThrough(transform) {
      if (!checkStreamFence(fencedReadableStreams, this)) {
        throw makeStreamFenceError();
      }
      if (isObjectOrFunc(transform)) {
        const w = ownDesc(transform, 'writable')?.value ?? ownDesc(transform, '_writable')?.value;
        if (!checkStreamFence(fencedWritableStreams, w)) {
          throw makeStreamFenceError();
        }
      }
      return reflectApply(readablePipeThrough, this, arguments);
    },
  );

  defineGuardedMethod(IntrinsicReadableStream.prototype, 'values', function values() {
    const checkOwner = getStreamFence(fencedReadableStreams, this);
    if (checkOwner !== undefined && !checkOwner()) {
      throw makeStreamFenceError();
    }
    const iter = reflectApply(readableValues, this, arguments);
    return checkOwner !== undefined ? wrapFencedAsyncIterator(iter, checkOwner) : iter;
  });

  if (typeof readableAsyncIterator === 'function') {
    defineGuardedMethod(
      IntrinsicReadableStream.prototype,
      Symbol.asyncIterator,
      function asyncIterator() {
        const checkOwner = getStreamFence(fencedReadableStreams, this);
        if (checkOwner !== undefined && !checkOwner()) {
          throw makeStreamFenceError();
        }
        const iter = reflectApply(readableAsyncIterator, this, arguments);
        return checkOwner !== undefined ? wrapFencedAsyncIterator(iter, checkOwner) : iter;
      },
    );
  }

  defineGuardedMethod(IntrinsicReadableStreamDefaultReader.prototype, 'read', function read() {
    if (!checkStreamFence(fencedReadableReaders, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(readerRead, this, arguments);
  });

  defineGuardedMethod(IntrinsicReadableStreamDefaultReader.prototype, 'cancel', function cancel() {
    if (!checkStreamFence(fencedReadableReaders, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(readerCancel, this, arguments);
  });

  defineGuardedMethod(
    IntrinsicReadableStreamDefaultReader.prototype,
    'releaseLock',
    function releaseLock() {
      if (!checkStreamFence(fencedReadableReaders, this)) {
        throw makeStreamFenceError();
      }
      return reflectApply(readerReleaseLock, this, arguments);
    },
  );

  defineGuardedMethod(IntrinsicWritableStream.prototype, 'abort', function abort() {
    if (!checkStreamFence(fencedWritableStreams, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(writableAbort, this, arguments);
  });

  defineGuardedMethod(IntrinsicWritableStream.prototype, 'close', function close() {
    if (!checkStreamFence(fencedWritableStreams, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(writableClose, this, arguments);
  });

  defineGuardedMethod(IntrinsicWritableStream.prototype, 'getWriter', function getWriter() {
    const checkOwner = getStreamFence(fencedWritableStreams, this);
    if (checkOwner !== undefined && !checkOwner()) {
      throw makeStreamFenceError();
    }
    const writer = reflectApply(writableGetWriter, this, arguments);
    if (checkOwner !== undefined && isObjectOrFunc(writer)) {
      weakMapSet.call(fencedWritableWriters, writer, checkOwner);
    }
    return writer;
  });

  defineGuardedMethod(IntrinsicWritableStreamDefaultWriter.prototype, 'write', function write() {
    if (!checkStreamFence(fencedWritableWriters, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(writerWrite, this, arguments);
  });

  defineGuardedMethod(IntrinsicWritableStreamDefaultWriter.prototype, 'close', function close() {
    if (!checkStreamFence(fencedWritableWriters, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(writerClose, this, arguments);
  });

  defineGuardedMethod(IntrinsicWritableStreamDefaultWriter.prototype, 'abort', function abort() {
    if (!checkStreamFence(fencedWritableWriters, this)) {
      return promiseReject(makeStreamFenceError());
    }
    return reflectApply(writerAbort, this, arguments);
  });

  defineGuardedMethod(
    IntrinsicWritableStreamDefaultWriter.prototype,
    'releaseLock',
    function releaseLock() {
      if (!checkStreamFence(fencedWritableWriters, this)) {
        throw makeStreamFenceError();
      }
      return reflectApply(writerReleaseLock, this, arguments);
    },
  );

  // Guard direct `new ReadableStreamDefaultReader(stream)` and
  // `new WritableStreamDefaultWriter(stream)` construction as well.
  const guardedReaderCtor = new IntrinsicProxy(IntrinsicReadableStreamDefaultReader, {
    construct(target, args, newTarget) {
      const stream = args.length > 0 ? args[0] : undefined;
      const checkOwner = getStreamFence(fencedReadableStreams, stream);
      if (checkOwner !== undefined && !checkOwner()) {
        throw makeStreamFenceError();
      }
      const reader = reflectConstruct(
        target,
        args,
        newTarget === guardedReaderCtor ? target : newTarget,
      );
      if (checkOwner !== undefined && isObjectOrFunc(reader)) {
        weakMapSet.call(fencedReadableReaders, reader, checkOwner);
      }
      return reader;
    },
  });
  const readerGlobalDesc = ownDesc(globalThis, 'ReadableStreamDefaultReader');
  if (readerGlobalDesc && readerGlobalDesc.configurable) {
    objectDefineProperty(globalThis, 'ReadableStreamDefaultReader', {
      value: guardedReaderCtor,
      writable: readerGlobalDesc.writable,
      enumerable: readerGlobalDesc.enumerable,
      configurable: readerGlobalDesc.configurable,
    });
  }

  const guardedWriterCtor = new IntrinsicProxy(IntrinsicWritableStreamDefaultWriter, {
    construct(target, args, newTarget) {
      const stream = args.length > 0 ? args[0] : undefined;
      const checkOwner = getStreamFence(fencedWritableStreams, stream);
      if (checkOwner !== undefined && !checkOwner()) {
        throw makeStreamFenceError();
      }
      const writer = reflectConstruct(
        target,
        args,
        newTarget === guardedWriterCtor ? target : newTarget,
      );
      if (checkOwner !== undefined && isObjectOrFunc(writer)) {
        weakMapSet.call(fencedWritableWriters, writer, checkOwner);
      }
      return writer;
    },
  });
  const writerGlobalDesc = ownDesc(globalThis, 'WritableStreamDefaultWriter');
  if (writerGlobalDesc && writerGlobalDesc.configurable) {
    objectDefineProperty(globalThis, 'WritableStreamDefaultWriter', {
      value: guardedWriterCtor,
      writable: writerGlobalDesc.writable,
      enumerable: writerGlobalDesc.enumerable,
      configurable: writerGlobalDesc.configurable,
    });
  }
}

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

// Returns 0 = transferable, 1 = locked, 2 = disturbed/errored, 3 = foreign request.
// F8: An unlocked, undisturbed, already-closed empty ReadableStream (`_state === 'closed'`)
// is transferable and preserves EOF semantics across the boundary.
function checkReadableTransferable(stream) {
  if (!isReadableStream(stream)) {
    return -1;
  }
  if (!checkStreamFence(fencedReadableStreams, stream)) {
    return 3;
  }
  if (readableLockedGetter.call(stream)) {
    return 1;
  }
  if (ownDesc(stream, '_disturbed')?.value === true) {
    return 2;
  }
  const state = ownDesc(stream, '_state')?.value;
  if (state !== undefined && state !== 'readable' && state !== 'closed') {
    return 2;
  }
  return 0;
}

// Returns 0 = transferable, 1 = locked, 2 = closed/errored, 3 = foreign request.
function checkWritableTransferable(stream) {
  if (!isWritableStream(stream)) {
    return -1;
  }
  if (!checkStreamFence(fencedWritableStreams, stream)) {
    return 3;
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
        promiseThen.call(
          p,
          () => {},
          () => {},
        );
      } catch {
        // Ignore cancel errors on already-terminated sources.
      }
    },
  };
}

function createReadableProxy(onPull, onCancel, checkOwner) {
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
  if (typeof checkOwner === 'function') {
    weakMapSet.call(fencedReadableStreams, stream, checkOwner);
  }
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
      promiseThen.call(
        p,
        () => onDone(),
        (err) => onError(err),
      );
    },
    close(onDone, onError) {
      let p;
      try {
        p = writerClose.call(writer);
      } catch (err) {
        onError(err);
        return;
      }
      promiseThen.call(
        p,
        () => onDone(),
        (err) => onError(err),
      );
    },
    abort(reason) {
      try {
        const p = writerAbort.call(writer, reason);
        promiseThen.call(
          p,
          () => {},
          () => {},
        );
      } catch {
        // Ignore abort errors on already-terminated sinks.
      }
    },
  };
}

function createWritableProxy(onWrite, onClose, onAbort, checkOwner) {
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
  if (typeof checkOwner === 'function') {
    weakMapSet.call(fencedWritableStreams, stream, checkOwner);
  }
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

// M4C / F8 helpers: unforgeable WeakSet brand checks registered at construction
// time in js/fetch.js (supporting genuine subclasses while rejecting spoofed
// objects) plus side-effect-free data-descriptor extraction.
function isRequest(value) {
  return isGenuineRequest(value);
}

function isResponse(value) {
  return isGenuineResponse(value);
}

function readDataProp(obj, key) {
  const desc = ownDesc(obj, key);
  if (desc === undefined) {
    return { present: false, accessor: false, value: undefined };
  }
  if (desc.get !== undefined || desc.set !== undefined) {
    return { present: true, accessor: true, value: undefined };
  }
  return { present: true, accessor: false, value: desc.value };
}

function readSafeStringSlot(obj, key, fallback) {
  const prop = readDataProp(obj, key);
  if (!prop.present) return { ok: true, value: fallback };
  if (prop.accessor || typeof prop.value !== 'string') return { ok: false, value: '' };
  return { ok: true, value: prop.value };
}

function readSafeOptionalStringSlot(obj, key) {
  const prop = readDataProp(obj, key);
  if (!prop.present || prop.value === null || prop.value === undefined) {
    return { ok: true, value: null };
  }
  if (prop.accessor || typeof prop.value !== 'string') return { ok: false, value: null };
  return { ok: true, value: prop.value };
}

function isBodyUsedSafely(obj) {
  const consumed = readDataProp(obj, '_bodyConsumed');
  if (consumed.accessor) return { ok: false, used: true };
  if (consumed.value === true) return { ok: true, used: true };
  const streamProp = readDataProp(obj, '_bodyStream');
  if (streamProp.accessor) return { ok: false, used: true };
  const stream = streamProp.value;
  if (stream !== null && stream !== undefined && isReadableStream(stream)) {
    const disturbed = readDataProp(stream, '_disturbed');
    if (disturbed.accessor) return { ok: false, used: true };
    if (disturbed.value === true) return { ok: true, used: true };
  }
  return { ok: true, used: false };
}

function inspectRequest(req) {
  if (!isRequest(req)) return null;
  const usedCheck = isBodyUsedSafely(req);
  if (!usedCheck.ok) return { error: 'Request internal state was tampered with' };
  if (usedCheck.used) {
    return { error: 'Request body is already used' };
  }

  const urlSlot = readSafeStringSlot(req, 'url', '');
  const methodSlot = readSafeStringSlot(req, 'method', 'GET');
  const credSlot = readSafeStringSlot(req, 'credentials', 'same-origin');
  const redirSlot = readSafeStringSlot(req, 'redirect', 'follow');
  const modeSlot = readSafeOptionalStringSlot(req, 'mode');
  if (!urlSlot.ok || !methodSlot.ok || !credSlot.ok || !redirSlot.ok || !modeSlot.ok) {
    return { error: 'Request metadata contains tampered properties' };
  }

  const rawHeadersProp = readDataProp(req, '_rawHeaders');
  const headersSlotProp = readDataProp(req, '_headers');
  if (rawHeadersProp.accessor || headersSlotProp.accessor) {
    return { error: 'Request headers state was tampered with' };
  }
  const headersObj = requestHeadersGetter.call(req);
  let headerPairs = [];
  if (headersObj !== null && headersObj !== undefined) {
    const ownedPairs = tryBridgeOwnedHeaderPairs(headersObj);
    if (ownedPairs === null) {
      return { error: 'Request headers is not a genuine Headers object' };
    }
    headerPairs = ownedPairs;
  }

  const signalSlotProp = readDataProp(req, '_signal');
  const signalStateProp = readDataProp(req, '_signalState');
  if (signalSlotProp.accessor || signalStateProp.accessor) {
    return { error: 'Request signal state was tampered with' };
  }
  const signal = requestSignalGetter.call(req);

  let bodyKind = 0; // 0 = none, 1 = inline bytes, 2 = stream
  let inlineBytes = null;
  let bodyStream = null;
  const fastKindProp = readDataProp(req, '_bodyFastKind');
  const fastDataProp = readDataProp(req, '_bodyFastData');
  const rawStreamProp = readDataProp(req, '_bodyStream');
  if (fastKindProp.accessor || fastDataProp.accessor || rawStreamProp.accessor) {
    return { error: 'Request body state was tampered with' };
  }
  const fastKind = fastKindProp.value ?? 0;
  const fastData = fastDataProp.value ?? null;
  const rawStream = rawStreamProp.value ?? null;

  if (fastKind === 1 && typeof fastData === 'string') {
    const encoded = textEncoderEncode.call(sharedTextEncoder, fastData);
    if (encoded.byteLength <= 65536) {
      bodyKind = 1;
      inlineBytes = encoded;
      req._bodyConsumed = true;
    } else {
      bodyKind = 2;
      bodyStream = requestBodyGetter.call(req);
    }
  } else if (fastKind === 2 && fastData !== null) {
    const view = extractBytes(fastData);
    if (view !== null && view.byteLength <= 65536) {
      bodyKind = 1;
      inlineBytes = view;
      req._bodyConsumed = true;
    } else {
      bodyKind = 2;
      bodyStream = requestBodyGetter.call(req);
    }
  } else if (rawStream !== null && rawStream !== undefined) {
    const check = checkReadableTransferable(rawStream);
    if (check === 1) return { error: 'Request body stream is locked' };
    if (check === 2) return { error: 'Request body stream is disturbed' };
    if (check === 3) {
      return {
        error: 'InvalidStateError: Cannot transfer Binding stream proxy outside its owning request',
      };
    }
    if (check !== 0) return { error: 'Request body stream is not transferable' };
    bodyKind = 2;
    bodyStream = rawStream;
  }

  return {
    url: urlSlot.value,
    method: methodSlot.value,
    credentials: credSlot.value,
    redirect: redirSlot.value,
    mode: modeSlot.value,
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
  const usedCheck = isBodyUsedSafely(res);
  if (!usedCheck.ok) return { error: 'Response internal state was tampered with' };
  if (usedCheck.used) {
    return { error: 'Response body is already used' };
  }

  const statusProp = readDataProp(res, 'status');
  if (
    statusProp.accessor ||
    (statusProp.present &&
      (typeof statusProp.value !== 'number' ||
        !numberIsInteger(statusProp.value) ||
        (statusProp.value !== 0 && (statusProp.value < 100 || statusProp.value > 599))))
  ) {
    return { error: 'Response status was tampered with' };
  }
  const status = statusProp.present ? statusProp.value : 200;

  const statusTextSlot = readSafeStringSlot(res, 'statusText', '');
  const urlSlot = readSafeStringSlot(res, 'url', '');
  const typeSlot = readSafeStringSlot(res, 'type', 'default');
  const redirProp = readDataProp(res, 'redirected');
  if (
    !statusTextSlot.ok ||
    !urlSlot.ok ||
    !typeSlot.ok ||
    redirProp.accessor ||
    (redirProp.present && typeof redirProp.value !== 'boolean')
  ) {
    return { error: 'Response metadata contains tampered properties' };
  }
  const redirected = redirProp.present ? redirProp.value : false;

  const headersSlotProp = readDataProp(res, '_headers');
  const headersStateProp = readDataProp(res, '_headersState');
  if (headersSlotProp.accessor || headersStateProp.accessor) {
    return { error: 'Response headers state was tampered with' };
  }
  const headersObj = responseHeadersGetter.call(res);
  let headerPairs = [];
  if (headersObj !== null && headersObj !== undefined) {
    const ownedPairs = tryBridgeOwnedHeaderPairs(headersObj);
    if (ownedPairs === null) {
      return { error: 'Response headers is not a genuine Headers object' };
    }
    headerPairs = ownedPairs;
  }

  let bodyKind = 0; // 0 = none, 1 = inline bytes, 2 = stream
  let inlineBytes = null;
  let bodyStream = null;
  const fastKindProp = readDataProp(res, '_bodyFastKind');
  const fastDataProp = readDataProp(res, '_bodyFastData');
  const rawStreamProp = readDataProp(res, '_bodyStream');
  if (fastKindProp.accessor || fastDataProp.accessor || rawStreamProp.accessor) {
    return { error: 'Response body state was tampered with' };
  }
  const fastKind = fastKindProp.value ?? 0;
  const fastData = fastDataProp.value ?? null;
  const rawStream = rawStreamProp.value ?? null;

  if (!isNullFastResponseBody(res)) {
    if (fastKind === 1 && typeof fastData === 'string') {
      const encoded = textEncoderEncode.call(sharedTextEncoder, fastData);
      if (encoded.byteLength <= 65536) {
        bodyKind = 1;
        inlineBytes = encoded;
        res._bodyConsumed = true;
      } else {
        bodyKind = 2;
        bodyStream = responseBodyGetter.call(res);
      }
    } else if (fastKind === 2 && fastData !== null) {
      const view = extractBytes(fastData);
      if (view !== null && view.byteLength <= 65536) {
        bodyKind = 1;
        inlineBytes = view;
        res._bodyConsumed = true;
      } else {
        bodyKind = 2;
        bodyStream = responseBodyGetter.call(res);
      }
    } else {
      const stream = getFastStreamResponseBody(res) ?? rawStream;
      if (stream !== null && stream !== undefined) {
        const check = checkReadableTransferable(stream);
        if (check === 1) return { error: 'Response body stream is locked' };
        if (check === 2) return { error: 'Response body stream is disturbed' };
        if (check === 3) {
          return {
            error:
              'InvalidStateError: Cannot transfer Binding stream proxy outside its owning request',
          };
        }
        if (check !== 0) return { error: 'Response body stream is not transferable' };
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
    statusText: statusTextSlot.value,
    url: urlSlot.value,
    redirected,
    type: typeSlot.value,
    headers: headerPairs,
    bodyKind,
    inlineBytes,
    bodyStream,
  };
}

function createProxyAbortController() {
  const controller = new IntrinsicAbortController();
  return {
    signal: controller.signal,
    abort(reason) {
      try {
        abortControllerAbort.call(controller, reason);
      } catch {
        // Ignore reentrant or already-aborted controller errors.
      }
    },
  };
}

function createProxyResponse(meta, body) {
  if (meta.type === 'error' || meta.status === 0) {
    // F9: Cross-boundary Response.error() must enforce immutable headers too.
    const errRes = IntrinsicResponse.error();
    markResponseHeadersImmutable(errRes);
    return errRes;
  }
  const headers = createHeadersFromIncomingPairs(meta.headers, false);
  const res = new IntrinsicResponse(body === undefined ? null : body, {
    status: meta.status,
    statusText: meta.statusText,
    headers,
  });
  if (meta.url) res.url = meta.url;
  if (meta.redirected) res.redirected = true;
  markResponseHeadersImmutable(res);
  return res;
}

// M4D: Unforgeable private-branded RpcTarget base class exposed ONLY on the
// Binding Runtime globalThis (never in User Runtime). Ordinary classes cannot
// forge the private `#rpcTarget` brand.
const IntrinsicSymbolDispose = Symbol.dispose;
const IntrinsicSymbolAsyncDispose = Symbol.asyncDispose;
const IntrinsicTypeError = TypeError;
const numberIsFinite = Number.isFinite;
const objectFreeze = Object.freeze;

class RpcTarget {
  #rpcTarget = true;
  #leaseMs = 0;

  constructor(options = undefined) {
    if (options !== undefined && options !== null) {
      if (typeof options !== 'object') {
        throw new IntrinsicTypeError('RpcTarget options must be an object');
      }
      const desc = ownDesc(options, 'leaseMs');
      if (desc !== undefined) {
        if (desc.get !== undefined || desc.set !== undefined) {
          throw new IntrinsicTypeError('RpcTarget leaseMs must be a data property');
        }
        const v = desc.value;
        if (v !== undefined) {
          if (typeof v !== 'number' || !numberIsFinite(v) || v < 0) {
            throw new IntrinsicTypeError('RpcTarget leaseMs must be a non-negative finite number');
          }
          this.#leaseMs = Math.floor(v);
        }
      }
    }
  }

  static _isInstance(value) {
    return value !== null && typeof value === 'object' && #rpcTarget in value;
  }

  static _getLeaseMs(value) {
    return #rpcTarget in value ? value.#leaseMs : 0;
  }
}

const isRpcTarget = RpcTarget._isInstance;
const getRpcTargetLeaseMs = RpcTarget._getLeaseMs;
delete RpcTarget._isInstance;
delete RpcTarget._getLeaseMs;
objectFreeze(RpcTarget.prototype);
objectFreeze(RpcTarget);

function disposeRpcTarget(target) {
  if (target === null || typeof target !== 'object') {
    return;
  }
  const invoke = (fn) => {
    try {
      const ret = fn.call(target);
      if (ret !== null && typeof ret === 'object' && typeof ret.then === 'function') {
        promiseThen.call(
          promiseResolve(ret),
          () => {},
          () => {},
        );
      }
    } catch {
      // Fault-isolated disposal: Binding cleanup failures never poison User or Worker.
    }
  };
  try {
    if (IntrinsicSymbolDispose !== undefined) {
      const fn = target[IntrinsicSymbolDispose];
      if (typeof fn === 'function') {
        invoke(fn);
        return;
      }
    }
    if (IntrinsicSymbolAsyncDispose !== undefined) {
      const fn = target[IntrinsicSymbolAsyncDispose];
      if (typeof fn === 'function') {
        invoke(fn);
        return;
      }
    }
    const fn = target.dispose;
    if (typeof fn === 'function') {
      invoke(fn);
    }
  } catch {
    // Ignore hostile property access during disposal.
  }
}

// F2: Bounded, cycle-safe, identity-deduped late-result disposal covering
// top-level and nested streams, Request/Response bodies, RpcTargets, and
// disposable resources inside supported plain objects and arrays.
const MAX_LATE_DISPOSE_DEPTH = 16;
const MAX_LATE_DISPOSE_NODES = 256;

function disposeLateValue(root) {
  if (root === null || typeof root !== 'object') {
    return;
  }
  const abortErr = new IntrinsicDOMException('Binding call was canceled', 'AbortError');
  const visited = new IntrinsicSet();
  let nodes = 0;

  function walk(value, depth) {
    if (value === null || typeof value !== 'object') {
      return;
    }
    if (depth > MAX_LATE_DISPOSE_DEPTH || nodes >= MAX_LATE_DISPOSE_NODES) {
      return;
    }
    if (setHas.call(visited, value)) {
      return;
    }
    setAdd.call(visited, value);
    nodes++;

    if (isReadableStream(value)) {
      try {
        if (!readableLockedGetter.call(value)) {
          const reader = readableGetReader.call(value);
          promiseThen.call(
            readerCancel.call(reader, abortErr),
            () => {},
            () => {},
          );
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
          promiseThen.call(
            writerAbort.call(writer, abortErr),
            () => {},
            () => {},
          );
        }
      } catch {
        // Best-effort bounded disposal.
      }
      return;
    }
    if (isResponse(value) || isRequest(value)) {
      const streamProp = readDataProp(value, '_bodyStream');
      if (!streamProp.accessor && streamProp.value !== null && isReadableStream(streamProp.value)) {
        walk(streamProp.value, depth + 1);
      }
      return;
    }
    if (isRpcTarget(value)) {
      disposeRpcTarget(value);
      return;
    }

    // Best-effort Symbol.dispose resource cleanup (bounded, fault-isolated).
    if (IntrinsicSymbolDispose !== undefined) {
      try {
        const fn = value[IntrinsicSymbolDispose];
        if (typeof fn === 'function') {
          const ret = fn.call(value);
          if (ret !== null && typeof ret === 'object' && typeof ret.then === 'function') {
            promiseThen.call(
              promiseResolve(ret),
              () => {},
              () => {},
            );
          }
        }
      } catch {
        // Fault-isolated disposal.
      }
    }

    // Recurse into supported plain containers using own data descriptors only
    // so hostile getters are never invoked during late-result cleanup.
    if (arrayIsArray(value)) {
      const lenProp = readDataProp(value, 'length');
      const len = !lenProp.accessor && typeof lenProp.value === 'number' ? lenProp.value : 0;
      const limit = len < MAX_LATE_DISPOSE_NODES ? len : MAX_LATE_DISPOSE_NODES;
      for (let i = 0; i < limit && nodes < MAX_LATE_DISPOSE_NODES; i++) {
        const elem = readDataProp(value, i);
        if (elem.present && !elem.accessor) {
          walk(elem.value, depth + 1);
        }
      }
      return;
    }
    let proto;
    try {
      proto = getProto(value);
    } catch {
      return;
    }
    if (proto === null || proto === ObjectProto) {
      let keys;
      try {
        keys = objectGetOwnPropertyNames(value);
      } catch {
        return;
      }
      for (let i = 0; i < keys.length && nodes < MAX_LATE_DISPOSE_NODES; i++) {
        const d = ownDesc(value, keys[i]);
        if (d !== undefined && d.get === undefined && d.set === undefined && d.enumerable) {
          walk(d.value, depth + 1);
        }
      }
    }
  }

  walk(root, 0);
}

export function installBindingRpcBridge(core) {
  if (typeof core.capsidInstallBindingBridge !== 'function') {
    return;
  }
  if (core.capsidEnableStreamFencing === true) {
    installStreamFenceGuards();
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
    createProxyAbortController,
    RpcTarget,
    isRpcTarget,
    getRpcTargetLeaseMs,
    disposeRpcTarget,
    disposeLateValue,
  });
}
