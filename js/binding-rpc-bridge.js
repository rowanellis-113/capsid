// Binding RPC v2 (v0.4.0): tamper-proof internal JS bridge for stream proxies,
// controlled Request/Response transfer, and late-result resource disposal.
//
// R6 (Zero-Binding Purity): Module evaluation performs ZERO bridge-specific
// allocations, WeakMap/TextEncoder construction, or intrinsic captures. All
// initialization runs lazily inside `installBindingRpcBridge(core)` ONLY when
// the worker runtime registers `core.capsidInstallBindingBridge` (i.e. at
// least one Binding is configured). Registered via the one-shot core hook and
// never exposed on globalThis.

import {
  createHeadersFromIncomingPairs,
  enableBindingHttpTracking,
  finalizeProxyResponse,
  getPrivateBodyStreamForBinding,
  isGenuineRequest,
  isGenuineResponse,
  markResponseHeadersImmutable,
  snapshotRequestForBinding,
  snapshotResponseForBinding,
} from './fetch.js';

let bridgeInstallCount = 0;

export function getBindingBridgeInitCount() {
  return bridgeInstallCount;
}

export function installBindingRpcBridge(core) {
  if (typeof core.capsidInstallBindingBridge !== 'function') {
    return;
  }
  bridgeInstallCount++;
  enableBindingHttpTracking();

  const ownDesc = Object.getOwnPropertyDescriptor;
  const getProto = Object.getPrototypeOf;
  const objectDefineProperty = Object.defineProperty;
  const objectGetOwnPropertyNames = Object.getOwnPropertyNames;
  const ObjectProto = Object.prototype;
  const arrayIsArray = Array.isArray;
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
  const IntrinsicReadableStreamBYOBReader =
    typeof ReadableStreamBYOBReader === 'function' ? ReadableStreamBYOBReader : null;
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
  const byobRead =
    IntrinsicReadableStreamBYOBReader !== null
      ? IntrinsicReadableStreamBYOBReader.prototype.read
      : null;
  const byobCancel =
    IntrinsicReadableStreamBYOBReader !== null
      ? IntrinsicReadableStreamBYOBReader.prototype.cancel
      : null;
  const byobReleaseLock =
    IntrinsicReadableStreamBYOBReader !== null
      ? IntrinsicReadableStreamBYOBReader.prototype.releaseLock
      : null;
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

  // F4/R3: Private ownership-fence registries for User-side Binding stream
  // proxies. Checked BEFORE js/streams.js mutates stream, reader, writer, or
  // controller state across EVERY supported public entry point (methods,
  // direct constructors, prototype.constructor, BYOB readers, and pipeThrough).
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

  function installGuardedConstructor(globalName, intrinsicCtor, streamFenceMap, handleFenceMap) {
    const guardedCtor = new IntrinsicProxy(intrinsicCtor, {
      construct(target, args, newTarget) {
        const stream = args.length > 0 ? args[0] : undefined;
        const checkOwner = getStreamFence(streamFenceMap, stream);
        if (checkOwner !== undefined && !checkOwner()) {
          throw makeStreamFenceError();
        }
        const instance = reflectConstruct(
          target,
          args,
          newTarget === guardedCtor ? target : newTarget,
        );
        if (checkOwner !== undefined && isObjectOrFunc(instance)) {
          weakMapSet.call(handleFenceMap, instance, checkOwner);
        }
        return instance;
      },
    });
    const globalDesc = ownDesc(globalThis, globalName);
    if (globalDesc && globalDesc.configurable) {
      objectDefineProperty(globalThis, globalName, {
        value: guardedCtor,
        writable: globalDesc.writable,
        enumerable: globalDesc.enumerable,
        configurable: globalDesc.configurable,
      });
    }
    // R3: Keep `prototype.constructor` synchronized with the guarded global
    // constructor so `new Reader.prototype.constructor(stream)` and
    // `new Writer.prototype.constructor(stream)` enforce the exact same fence.
    const proto = intrinsicCtor.prototype;
    const protoCtorDesc = ownDesc(proto, 'constructor');
    if (protoCtorDesc && protoCtorDesc.configurable) {
      objectDefineProperty(proto, 'constructor', {
        value: guardedCtor,
        writable: protoCtorDesc.writable,
        enumerable: protoCtorDesc.enumerable,
        configurable: protoCtorDesc.configurable,
      });
    }
    return guardedCtor;
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

  // Installed during User bootstrap ONLY when the worker loads at least one
  // Binding (never on zero-Binding workers or Binding runtimes). Runs before
  // any application module evaluates so saved prototype references are guarded.
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

    // R3: Snapshot `readable` and `writable` from `rawTransform` ONCE, enforce
    // ownership fences on the exact snapshotted objects, and forward a plain
    // null-proto pair to `readablePipeThrough` so getter flip-flopping can
    // never smuggle an unfenced stream past pre-checks.
    defineGuardedMethod(
      IntrinsicReadableStream.prototype,
      'pipeThrough',
      function pipeThrough(rawTransform) {
        if (!checkStreamFence(fencedReadableStreams, this)) {
          throw makeStreamFenceError();
        }
        if (!isReadableStream(this) || !isObjectOrFunc(rawTransform)) {
          return reflectApply(readablePipeThrough, this, arguments);
        }
        const readable = rawTransform.readable;
        const writable = rawTransform.writable;
        if (
          !checkStreamFence(fencedWritableStreams, writable) ||
          !checkStreamFence(fencedReadableStreams, readable)
        ) {
          throw makeStreamFenceError();
        }
        const snapshot = { __proto__: null, readable, writable };
        return arguments.length > 1
          ? reflectApply(readablePipeThrough, this, [snapshot, arguments[1]])
          : reflectApply(readablePipeThrough, this, [snapshot]);
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

    defineGuardedMethod(
      IntrinsicReadableStreamDefaultReader.prototype,
      'cancel',
      function cancel() {
        if (!checkStreamFence(fencedReadableReaders, this)) {
          return promiseReject(makeStreamFenceError());
        }
        return reflectApply(readerCancel, this, arguments);
      },
    );

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

    if (IntrinsicReadableStreamBYOBReader !== null) {
      if (typeof byobRead === 'function') {
        defineGuardedMethod(IntrinsicReadableStreamBYOBReader.prototype, 'read', function read() {
          if (!checkStreamFence(fencedReadableReaders, this)) {
            return promiseReject(makeStreamFenceError());
          }
          return reflectApply(byobRead, this, arguments);
        });
      }
      if (typeof byobCancel === 'function') {
        defineGuardedMethod(
          IntrinsicReadableStreamBYOBReader.prototype,
          'cancel',
          function cancel() {
            if (!checkStreamFence(fencedReadableReaders, this)) {
              return promiseReject(makeStreamFenceError());
            }
            return reflectApply(byobCancel, this, arguments);
          },
        );
      }
      if (typeof byobReleaseLock === 'function') {
        defineGuardedMethod(
          IntrinsicReadableStreamBYOBReader.prototype,
          'releaseLock',
          function releaseLock() {
            if (!checkStreamFence(fencedReadableReaders, this)) {
              throw makeStreamFenceError();
            }
            return reflectApply(byobReleaseLock, this, arguments);
          },
        );
      }
    }

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

    installGuardedConstructor(
      'ReadableStreamDefaultReader',
      IntrinsicReadableStreamDefaultReader,
      fencedReadableStreams,
      fencedReadableReaders,
    );
    if (IntrinsicReadableStreamBYOBReader !== null) {
      installGuardedConstructor(
        'ReadableStreamBYOBReader',
        IntrinsicReadableStreamBYOBReader,
        fencedReadableStreams,
        fencedReadableReaders,
      );
    }
    installGuardedConstructor(
      'WritableStreamDefaultWriter',
      IntrinsicWritableStreamDefaultWriter,
      fencedWritableStreams,
      fencedWritableWriters,
    );
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

  // M4C / R4: One-shot private-slot snapshots owned by js/fetch.js (zero
  // mutable property reads or user callbacks on accept or reject paths).
  function isRequest(value) {
    return isGenuineRequest(value);
  }

  function isResponse(value) {
    return isGenuineResponse(value);
  }

  function inspectRequest(req) {
    return snapshotRequestForBinding(req, checkReadableTransferable, extractBytes);
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
    return snapshotResponseForBinding(res, checkReadableTransferable, extractBytes);
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
    finalizeProxyResponse(res, meta.url, meta.redirected);
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
              throw new IntrinsicTypeError(
                'RpcTarget leaseMs must be a non-negative finite number',
              );
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
        const bodyStream = getPrivateBodyStreamForBinding(value);
        if (bodyStream !== null && isReadableStream(bodyStream)) {
          walk(bodyStream, depth + 1);
        }
        return;
      }
      if (isRpcTarget(value)) {
        disposeRpcTarget(value);
        return;
      }

      // Best-effort Symbol.dispose / Symbol.asyncDispose resource cleanup
      // (bounded across both sync and async continuations via Binding async
      // disposal deadline propagation).
      const trySymbolDispose = (sym) => {
        if (sym === undefined) return false;
        try {
          const fn = value[sym];
          if (typeof fn === 'function') {
            const ret = fn.call(value);
            if (ret !== null && typeof ret === 'object' && typeof ret.then === 'function') {
              promiseThen.call(
                promiseResolve(ret),
                () => {},
                () => {},
              );
            }
            return true;
          }
        } catch {
          // Fault-isolated disposal.
        }
        return false;
      };
      if (!trySymbolDispose(IntrinsicSymbolDispose)) {
        trySymbolDispose(IntrinsicSymbolAsyncDispose);
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
