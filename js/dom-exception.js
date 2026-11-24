import core from 'tjs:internal/core';

// QuickJS-ng provides the DOMException implementation and brand checks, but
// its Web IDL attributes are not enumerable. Web IDL requires interface
// prototype attributes to be enumerable.
for (const name of ['name', 'message', 'code']) {
  const descriptor = Object.getOwnPropertyDescriptor(DOMException.prototype, name);

  if (descriptor) {
    Object.defineProperty(DOMException.prototype, name, {
      ...descriptor,
      enumerable: true,
    });
  }
}

function toWebIdlDouble(value, label) {
  const number = +value;
  if (!Number.isFinite(number)) {
    throw new TypeError(`${label} must be a finite number`);
  }
  return number;
}

const domExceptionNameGetter = Object.getOwnPropertyDescriptor(DOMException.prototype, 'name')?.get;
const domExceptionMessageGetter = Object.getOwnPropertyDescriptor(
  DOMException.prototype,
  'message',
)?.get;

export class QuotaExceededError extends DOMException {
  #quota;
  #requested;

  static #hasBrand(value) {
    return value !== null && typeof value === 'object' && #quota in value;
  }

  static _capture(value) {
    if (!QuotaExceededError.#hasBrand(value)) {
      return null;
    }
    return {
      message: domExceptionMessageGetter ? domExceptionMessageGetter.call(value) : value.message,
      quota: value.#quota,
      requested: value.#requested,
    };
  }

  constructor(message = '', options = {}) {
    const text = message === undefined ? '' : `${message}`;
    let quota = null;
    let requested = null;
    if (options !== undefined && options !== null) {
      if (typeof options !== 'object' && typeof options !== 'function') {
        throw new TypeError('QuotaExceededError options must be an object');
      }
      const rawQuota = options.quota;
      if (rawQuota !== undefined) {
        quota = toWebIdlDouble(rawQuota, 'QuotaExceededError quota');
      }
      const rawRequested = options.requested;
      if (rawRequested !== undefined) {
        requested = toWebIdlDouble(rawRequested, 'QuotaExceededError requested');
      }
    }
    if (quota !== null && quota < 0) {
      throw new RangeError('QuotaExceededError quota must not be negative');
    }
    if (requested !== null && requested < 0) {
      throw new RangeError('QuotaExceededError requested must not be negative');
    }
    if (quota !== null && requested !== null && requested < quota) {
      throw new RangeError('QuotaExceededError requested must not be less than quota');
    }
    super(text, 'QuotaExceededError');
    this.#quota = quota;
    this.#requested = requested;
  }

  get quota() {
    return this.#quota;
  }

  get requested() {
    return this.#requested;
  }
}

for (const name of ['quota', 'requested']) {
  const descriptor = Object.getOwnPropertyDescriptor(QuotaExceededError.prototype, name);
  if (descriptor) {
    Object.defineProperty(QuotaExceededError.prototype, name, {
      ...descriptor,
      enumerable: true,
    });
  }
}

Object.defineProperty(QuotaExceededError.prototype, Symbol.toStringTag, {
  configurable: true,
  value: 'QuotaExceededError',
});

Object.defineProperty(globalThis, 'QuotaExceededError', {
  configurable: true,
  enumerable: false,
  writable: true,
  value: QuotaExceededError,
});

const captureQuota = QuotaExceededError._capture;
function cloneDomException(source) {
  const record = captureQuota(source);
  if (record === null) {
    return new DOMException(
      domExceptionMessageGetter.call(source),
      domExceptionNameGetter.call(source),
    );
  }
  const options = {};
  if (record.quota !== null) options.quota = record.quota;
  if (record.requested !== null) options.requested = record.requested;
  return new QuotaExceededError(record.message, options);
}

export function installStructuredCloneDomExceptionSupport() {
  const cloneNative = globalThis.structuredClone;
  const classOf = core.objectClass;
  const ordinary = classOf({});
  const array = classOf([]);
  const map = classOf(new Map());
  const set = classOf(new Set());
  const exception = classOf(new DOMException());
  const dataView = classOf(new DataView(new ArrayBuffer(0)));
  const getter = (proto, key) => Object.getOwnPropertyDescriptor(proto, key).get;
  const detached = getter(ArrayBuffer.prototype, 'detached');
  const typed = Object.getPrototypeOf(Uint8Array.prototype);
  const typedBuffer = getter(typed, 'buffer');
  const typedOffset = getter(typed, 'byteOffset');
  const typedLength = getter(typed, 'length');
  const dataBuffer = getter(DataView.prototype, 'buffer');
  const dataOffset = getter(DataView.prototype, 'byteOffset');
  const dataLength = getter(DataView.prototype, 'byteLength');
  const viewConstructors = new Map([[dataView, DataView]]);
  for (const ctor of [
    Int8Array,
    Uint8Array,
    Uint8ClampedArray,
    Int16Array,
    Uint16Array,
    Int32Array,
    Uint32Array,
    Float32Array,
    Float64Array,
    BigInt64Array,
    BigUint64Array,
    globalThis.Float16Array,
  ]) {
    if (ctor) viewConstructors.set(classOf(new ctor(0)), ctor);
  }

  const cloneError = (message) => new DOMException(message, 'DataCloneError');
  const defineProperty = Object.defineProperty;
  const ownKeys = Object.keys;
  const hasOwn = Object.hasOwn;
  const mapEntries = Map.prototype.entries;
  const setValues = Set.prototype.values;
  const mapSet = Map.prototype.set;
  const setAdd = Set.prototype.add;

  function structuredClone(value, options = {}) {
    if (arguments.length === 0) throw new TypeError('structuredClone requires a value');
    const transfers = new Set();
    if (options != null && typeof options !== 'object' && typeof options !== 'function') {
      throw new TypeError('structuredClone options must be a dictionary');
    }
    const list = options?.transfer;
    if (list !== undefined) {
      if (list === null || (typeof list !== 'object' && typeof list !== 'function')) {
        throw new TypeError('transfer must be a sequence of objects');
      }
      const items = [];
      for (const item of list) {
        if (item === null || (typeof item !== 'object' && typeof item !== 'function')) {
          throw new TypeError('transfer entries must be objects');
        }
        items.push(item);
      }
      for (const buffer of items) {
        if (!core.isArrayBuffer(buffer) || transfers.has(buffer)) {
          throw cloneError('Expected distinct ArrayBuffers in transfer list');
        }
        transfers.add(buffer);
      }
    }

    // Capture the graph before detaching anything. Records carry clone identity across
    // cycles. Container links are installed after transferred buffers and views exist.
    const seen = new Map();
    const views = [];
    const links = [];
    function capture(source) {
      if (source === null || typeof source !== 'object') {
        if (typeof source === 'symbol' || typeof source === 'function') {
          throw cloneError('Value cannot be cloned');
        }
        return { copy: source };
      }
      if (seen.has(source)) return seen.get(source);
      const record = { copy: undefined };
      seen.set(source, record);
      if (transfers.has(source)) return record;
      const kind = classOf(source);
      if (kind === ordinary || kind === array) {
        const copy = kind === array ? new Array(source.length) : {};
        record.copy = copy;
        for (const key of ownKeys(source)) {
          if (!hasOwn(source, key)) continue;
          const child = capture(source[key]);
          links.push(() =>
            defineProperty(copy, key, {
              value: child.copy,
              writable: true,
              enumerable: true,
              configurable: true,
            }),
          );
        }
      } else if (kind === map) {
        const copy = new Map();
        record.copy = copy;
        for (const [k, v] of [...mapEntries.call(source)]) {
          const key = capture(k);
          const entry = capture(v);
          links.push(() => mapSet.call(copy, key.copy, entry.copy));
        }
      } else if (kind === set) {
        const copy = new Set();
        record.copy = copy;
        for (const v of [...setValues.call(source)]) {
          const entry = capture(v);
          links.push(() => setAdd.call(copy, entry.copy));
        }
      } else if (kind === exception) {
        record.copy = cloneDomException(source);
      } else if (viewConstructors.has(kind)) {
        const isData = kind === dataView;
        const buffer = (isData ? dataBuffer : typedBuffer).call(source);
        if (core.isArrayBuffer(buffer) && detached.call(buffer)) {
          throw cloneError('Cannot clone a detached view');
        }
        const backing = capture(buffer);
        const offset = (isData ? dataOffset : typedOffset).call(source);
        const length = (isData ? dataLength : typedLength).call(source);
        const ctor = viewConstructors.get(kind);
        views.push(() => {
          record.copy = new ctor(backing.copy, offset, length);
        });
      } else {
        record.copy = cloneNative(source);
      }
      return record;
    }

    const root = capture(value);
    for (const buffer of transfers) {
      // An accessor encountered during capture may have detached a listed buffer.
      if (detached.call(buffer)) throw cloneError('Transfer buffer was detached during clone');
      if (seen.has(buffer)) seen.get(buffer).copy = cloneNative(buffer);
      core.detachArrayBuffer(buffer);
    }
    for (const createView of views) createView();
    for (const link of links) link();
    return root.copy;
  }

  Object.defineProperty(globalThis, 'structuredClone', {
    configurable: true,
    enumerable: true,
    writable: true,
    value: structuredClone,
  });
}
