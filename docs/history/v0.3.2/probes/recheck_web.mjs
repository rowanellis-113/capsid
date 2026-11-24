// Additional Web IDL and serialization cases for test-worker-integration's platform mode.
export default {
  fetch() {
    const values = {};
    const failures = [];
    const Q = QuotaExceededError;
    function check(name, read, expected) {
      let actual;
      try {
        actual = read();
      } catch (e) {
        actual = 'threw ' + e.name;
      }
      values[name] = actual;
      if (actual !== expected) failures.push({ name, actual, expected });
    }
    const err = () => new Q('x', { quota: 10, requested: 20 });
    check(
      'nested graph aliases',
      () => {
        const e = err(),
          a = Object.create(null);
        a.map = new Map([[e, new Set([e])]]);
        a.self = a;
        const c = structuredClone(a);
        const [[k, v]] = c.map;
        return c.self === c && v.has(k) && k.quota === 10;
      },
      true,
    );
    check(
      'array own property',
      () => {
        const a = [err()];
        a.label = 'records';
        return structuredClone(a).label ?? 'missing';
      },
      'records',
    );
    check(
      'array holes',
      () => {
        const a = new Array(3);
        a[2] = err();
        const c = structuredClone(a);
        return [c.length, Object.keys(c).join(','), c[2].requested].join('/');
      },
      '3/2/20',
    );
    check(
      'custom class containing exception',
      () => {
        class Record {
          constructor() {
            this.error = err();
          }
        }
        const c = structuredClone(new Record());
        return c.error.quota;
      },
      10,
    );
    check(
      'getter observed once',
      () => {
        let reads = 0;
        const c = structuredClone({
          get index() {
            return ++reads;
          },
          error: err(),
        });
        return reads + '/' + c.index;
      },
      '1/1',
    );
    check(
      'abrupt getter preserves exception',
      () => {
        const failure = new RangeError('first');
        let calls = 0;
        try {
          structuredClone({
            get index() {
              if (++calls === 1) throw failure;
              return err();
            },
          });
        } catch (e) {
          return e === failure && calls === 1;
        }
        return false;
      },
      true,
    );
    check(
      'mixed uncloneable',
      () => structuredClone({ e: err(), f: () => {} }),
      'threw DataCloneError',
    );
    check(
      'top transfer',
      () => {
        const b = new ArrayBuffer(3);
        const c = structuredClone(err(), { transfer: [b] });
        return b.byteLength === 0 && c.quota === 10;
      },
      true,
    );
    check(
      'duplicate transfer list',
      () => {
        const b = new ArrayBuffer(3);
        try {
          structuredClone(err(), { transfer: [b, b] });
        } catch (e) {
          return e.name + '/' + b.byteLength;
        }
        return 'accepted/' + b.byteLength;
      },
      'DataCloneError/3',
    );
    check(
      'already detached transfer',
      () => {
        const b = new ArrayBuffer(3);
        structuredClone(b, { transfer: [b] });
        return structuredClone(err(), { transfer: [b] }).name;
      },
      'threw DataCloneError',
    );
    check(
      'non-transferable',
      () => structuredClone(err(), { transfer: [{}] }),
      'threw DataCloneError',
    );
    check(
      'nested transfer accessed once',
      () => {
        let reads = 0;
        const b = new ArrayBuffer(3);
        const c = structuredClone(
          { error: err(), b },
          {
            get transfer() {
              reads++;
              return [b];
            },
          },
        );
        return reads + '/' + c.b.byteLength + '/' + b.byteLength;
      },
      '1/3/0',
    );
    check(
      'ordinary object with exception prototype',
      () => {
        const x = Object.create(Q.prototype);
        x.label = 4;
        const c = structuredClone(x);
        return !(c instanceof Q) && c.label === 4;
      },
      true,
    );
    for (const [name, v] of [
      ['undefined', undefined],
      ['null', null],
      ['object', {}],
      ['buffer', new ArrayBuffer(0)],
    ])
      check('crypto ' + name, () => crypto.getRandomValues(v), 'threw TypeError');
    for (const [name, v] of [
      ['dataview', new DataView(new ArrayBuffer(0))],
      ['float32', new Float32Array(0)],
      ['float64', new Float64Array(0)],
    ])
      check('crypto ' + name, () => crypto.getRandomValues(v), 'threw TypeMismatchError');
    check(
      'crypto zero',
      () => {
        const v = new Uint8Array(0);
        return crypto.getRandomValues(v) === v;
      },
      true,
    );
    check(
      'crypto detached originally empty',
      () => {
        const v = new Uint8Array(0);
        structuredClone(v.buffer, { transfer: [v.buffer] });
        return crypto.getRandomValues(v) === v;
      },
      true,
    );
    check(
      'crypto shared integer view',
      () => {
        const v = new Uint8Array(new SharedArrayBuffer(1));
        return crypto.getRandomValues(v) === v;
      },
      'threw TypeError',
    );
    check(
      'crypto detached integer view diagnostic',
      () => {
        const v = new Uint8Array(1);
        structuredClone(v.buffer, { transfer: [v.buffer] });
        return crypto.getRandomValues(v) === v;
      },
      true,
    );
    check(
      'crypto shadowed subview',
      () => {
        const b = new Uint8Array(4).fill(107);
        const v = b.subarray(2, 3);
        Object.defineProperties(v, {
          buffer: { value: new ArrayBuffer(0) },
          byteOffset: { value: 0 },
          byteLength: { value: 99999 },
        });
        return crypto.getRandomValues(v) === v && b[0] === 107 && b[1] === 107 && b[3] === 107;
      },
      true,
    );
    check(
      'crypto 65537',
      () => crypto.getRandomValues(new Uint8Array(65537)),
      'threw QuotaExceededError',
    );
    return Response.json({
      profile: 'CAPSID-MIN-2025-subset-v0',
      passed: failures.length === 0,
      failures,
      values,
    });
  },
};
