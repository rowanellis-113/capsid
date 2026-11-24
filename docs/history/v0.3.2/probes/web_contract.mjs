// Run in test-worker-integration's platform mode against a real Capsid worker.
export default {
  fetch() {
    const failures = [];
    const values = {};
    const check = (name, read, expected) => {
      let actual;
      try { actual = read(); } catch (error) { actual = 'threw ' + error.name; }
      values[name] = actual;
      if (!Object.is(actual, expected)) failures.push({ name, expected, actual });
    };
    const error = new QuotaExceededError('x', {quota: 10, requested: 20});
    check('clone preserves type, message and fields', () => {
      const copy = structuredClone(error);
      return copy !== error && copy.constructor === QuotaExceededError && copy.message === 'x' &&
        copy.code === 22 && copy.quota === 10 && copy.requested === 20;
    }, true);
    check('quota preserves signed zero', () => Object.is(new QuotaExceededError('', {quota: -0}).quota, -0), true);
    check('undefined is rejected by the ArrayBufferView binding', () => crypto.getRandomValues(), 'threw TypeError');
    check('ordinary object is rejected by the ArrayBufferView binding', () => crypto.getRandomValues({}), 'threw TypeError');
    check('Float32Array gets TypeMismatchError', () => crypto.getRandomValues(new Float32Array(1)), 'threw TypeMismatchError');
    check('valid small view uses its internal byte length', () => {
      const view = new Uint8Array(1);
      Object.defineProperty(view, 'byteLength', {value: 65537});
      return crypto.getRandomValues(view) === view;
    }, true);
    check('quota exception for a genuine oversized integer view', () => {
      try { crypto.getRandomValues(new Uint8Array(65537)); } catch (e) {
        return e.constructor === QuotaExceededError && e.code === 22 && e.quota === null && e.requested === null;
      }
      return false;
    }, true);
    return Response.json({ profile: 'CAPSID-MIN-2025-subset-v0', passed: failures.length === 0, failures, values });
  },
};
