import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import { test } from 'node:test';

const adapter = fs.readFileSync(new URL('./adapter.js', import.meta.url), 'utf8');
const upstreamPath = process.env.CAPSID_WPT_HARNESS;
const upstream = upstreamPath ? fs.readFileSync(upstreamPath, 'utf8') : null;

async function outcome(source, expression) {
    const context = vm.createContext({ setTimeout, clearTimeout, DOMException });
    vm.runInContext('globalThis.self = globalThis;', context);
    vm.runInContext(`
        globalThis.QuotaExceededError = class QuotaExceededError extends DOMException {
            constructor(requested, quota) {
                super('quota', 'QuotaExceededError');
                Object.assign(this, { requested, quota });
            }
        };
    `, context);
    vm.runInContext(source, context);
    return vm.runInContext(`(async () => {
        try { await (${expression}); return 'PASS'; }
        catch (error) { return error instanceof AssertionError ? 'AssertionError' : error.name; }
    })()`, context);
}

// Expected outcomes follow the locked WPT testharness.js. When WPT is present,
// run that independent harness too; the adapter must not define its own oracle.
const cases = [
    ['quota exact fields', 'assert_throws_quotaexceedederror(() => { throw new QuotaExceededError(7, 5); }, 7, 5)', 'PASS'],
    ['quota explicit realm', 'assert_throws_quotaexceedederror(QuotaExceededError, () => { throw new QuotaExceededError(null, null); }, null, null)', 'PASS'],
    ['quota predicates', 'assert_throws_quotaexceedederror(() => { throw new QuotaExceededError(7, 5); }, n => n > 5, n => n === 5)', 'PASS'],
    ['generic named exception', 'assert_throws_quotaexceedederror(() => { throw Object.assign(new DOMException("quota", "QuotaExceededError"), {requested: 7, quota: 5}); }, 7, 5)', 'AssertionError'],
    ['missing nullable property', 'assert_throws_quotaexceedederror(() => { const e = new QuotaExceededError(null, null); delete e.quota; throw e; }, null, null)', 'AssertionError'],
    ['incorrect requested amount', 'assert_throws_quotaexceedederror(() => { throw new QuotaExceededError(7, 5); }, 8, 5)', 'AssertionError'],
    ['incorrect quota predicate', 'assert_throws_quotaexceedederror(() => { throw new QuotaExceededError(7, 5); }, 7, n => n > 5)', 'AssertionError'],
    ['quota callback succeeds', 'assert_throws_quotaexceedederror(() => {}, null, null)', 'AssertionError'],
    ['approximate finite match', 'assert_approx_equals(1, 1.01, .02)', 'PASS'],
    ['approximate finite mismatch', 'assert_approx_equals(1, 2, .02)', 'AssertionError'],
    ['NaN actual', 'assert_approx_equals(NaN, 1, .02)', 'AssertionError'],
    ['NaN expected', 'assert_approx_equals(1, NaN, .02)', 'AssertionError'],
    ['NaN tolerance', 'assert_approx_equals(1, 1, NaN)', 'AssertionError'],
    ['NaN pair', 'assert_approx_equals(NaN, NaN, 0)', 'PASS'],
    ['infinity pair', 'assert_approx_equals(Infinity, Infinity, 0)', 'PASS'],
    ['opposite infinities', 'assert_approx_equals(Infinity, -Infinity, 0)', 'AssertionError'],
    ['approximate expected coercion', 'assert_approx_equals(1, "1", 0)', 'PASS'],
    ['equal dense arrays', 'assert_array_equals([1, undefined], [1, undefined])', 'PASS'],
    ['equal sparse arrays', 'assert_array_equals(Array(1), Array(1))', 'PASS'],
    ['hole versus undefined', 'assert_array_equals(Array(1), [undefined])', 'AssertionError'],
    ['undefined versus hole', 'assert_array_equals([undefined], Array(1))', 'AssertionError'],
    ['non-array objects', 'assert_array_equals({}, {})', 'AssertionError'],
    ['string versus array', 'assert_array_equals("a", ["a"])', 'AssertionError'],
    ['exact error constructor', 'assert_throws_js(TypeError, () => { throw new TypeError("expected"); })', 'PASS'],
    ['error subclass mismatch', 'assert_throws_js(Error, () => { throw new TypeError("wrong class"); })', 'AssertionError'],
    ['changed error name', 'assert_throws_js(TypeError, () => { const e = new TypeError(); e.name = "OtherError"; throw e; })', 'AssertionError'],
    ['non-Error constructor', 'assert_throws_js(Object, () => { throw {}; })', 'AssertionError'],
    ['nested assertion', 'assert_throws_js(Error, () => assert_true(false))', 'AssertionError'],
    ['exact rejected constructor', 'promise_rejects_js(null, TypeError, Promise.reject(new TypeError()))', 'PASS'],
    ['rejected subclass mismatch', 'promise_rejects_js(null, Error, Promise.reject(new TypeError()))', 'AssertionError'],
    ['rejected nested assertion', 'promise_rejects_js(null, Error, Promise.resolve().then(() => assert_true(false)))', 'AssertionError'],
    ['greater numbers', 'assert_greater_than(2, 1)', 'PASS'],
    ['greater bigints', 'assert_greater_than(2n, 1n)', 'PASS'],
    ['greater string input', 'assert_greater_than("2", 1)', 'AssertionError'],
    ['greater mixed numeric types', 'assert_greater_than(2n, 1)', 'AssertionError'],
    ['array member', 'assert_in_array(1, [1])', 'PASS'],
    ['NaN array member', 'assert_in_array(NaN, [NaN])', 'AssertionError'],
    ['array hole is not a member', 'assert_in_array(undefined, Array(1))', 'AssertionError'],
    ['exact thrown NaN', 'assert_throws_exactly(NaN, () => { throw NaN; })', 'PASS'],
    ['exact thrown signed zero', 'assert_throws_exactly(-0, () => { throw 0; })', 'AssertionError'],
    ['exact thrown assertion', '(() => { const e = new AssertionError("inner"); assert_throws_exactly(e, () => { throw e; }); })()', 'AssertionError'],
    ['exact rejected NaN', 'promise_rejects_exactly(null, NaN, Promise.reject(NaN))', 'PASS'],
    ['exact rejected signed zero', 'promise_rejects_exactly(null, -0, Promise.reject(0))', 'AssertionError'],
    ['exact rejected assertion', '(() => { const e = new AssertionError("inner"); return promise_rejects_exactly(null, e, Promise.reject(e)); })()', 'AssertionError'],
];

for (const [name, expression, expected] of cases) {
    test(name, async () => {
        if (upstream) {
            // promise_rejects_js needs the harness Test's unreached callback.
            const referenceExpression = expression.replace(/(promise_rejects_(?:js|exactly))\(null,/,
                '$1({ unreached_func: () => () => assert_unreached() },');
            assert.equal(await outcome(upstream, referenceExpression), expected, 'locked upstream outcome');
        }
        assert.equal(await outcome(adapter, expression), expected, 'Capsid adapter outcome');
    });
}

test('adapter distinguishes assertion, cleanup and timeout failures', async () => {
    const timers = [];
    const context = vm.createContext({ setTimeout: fn => { timers.push(fn); return timers.length; }, clearTimeout() {} });
    vm.runInContext(adapter, context);
    vm.runInContext(`
        test(() => assert_true(false), 'assertion');
        test(t => t.add_cleanup(() => { throw new TypeError('cleanup broke'); }), 'cleanup');
        async_test('timeout');
        __wptSeal();
    `, context);
    timers[0]();
    const results = JSON.parse(JSON.stringify(await context.__wptCompletion));
    assert.deepEqual(results.map(({ name, status, errorName, phase }) => ({ name, status, errorName, phase })), [
        { name: 'assertion', status: 'FAIL', errorName: 'AssertionError', phase: 'test' },
        { name: 'cleanup', status: 'FAIL', errorName: 'TypeError', phase: 'cleanup' },
        { name: 'timeout', status: 'TIMEOUT', errorName: 'TimeoutError', phase: 'timeout' },
    ]);
    assert.match(results[1].message, /cleanup broke/);
});

test('cleanup diagnostics retain the message when stack contains only frames', async () => {
    const context = vm.createContext({ setTimeout, clearTimeout });
    vm.runInContext(adapter, context);
    vm.runInContext(`
        test(t => t.add_cleanup(() => {
            const error = new TypeError('cleanup diagnostic must survive');
            error.stack = '    at cleanup.js:1:1';
            throw error;
        }), 'cleanup message');
        __wptSeal();
    `, context);
    const [result] = JSON.parse(JSON.stringify(await context.__wptCompletion));
    assert.equal(result.status, 'FAIL');
    assert.equal(result.phase, 'cleanup');
    assert.match(result.message, /cleanup diagnostic must survive/);
    assert.match(result.message, /at cleanup.js:1:1/);
});

test('promise tests wait for preceding cleanup and require a thenable', async () => {
    const context = vm.createContext({ setTimeout, clearTimeout });
    vm.runInContext(adapter, context);
    vm.runInContext(`
        const events = [];
        promise_test(async t => {
            t.add_cleanup(() => new Promise(resolve => {
                setTimeout(() => { events.push('clean'); resolve(); }, 0);
            }));
            events.push('first');
        }, 'first');
        promise_test(async () => assert_array_equals(events, ['first', 'clean']), 'second');
        promise_test(() => {}, 'missing return');
        promise_test(async t => {
            t.add_cleanup(async () => { throw new Error('cleanup rejected'); });
        }, 'bad cleanup');
        __wptSeal();
    `, context);
    const results = JSON.parse(JSON.stringify(await context.__wptCompletion));
    assert.deepEqual(results.map(r => [r.name, r.status, r.phase]), [
        ['first', 'PASS', 'test'], ['second', 'PASS', 'test'],
        ['missing return', 'FAIL', 'test'], ['bad cleanup', 'FAIL', 'cleanup'],
    ]);
    assert.match(results[3].message, /cleanup rejected/);
});

test('a stuck promise or cleanup times out and releases the next test', async () => {
    const context = vm.createContext({
        setTimeout: callback => setTimeout(callback, 15), clearTimeout,
    });
    vm.runInContext(adapter, context);
    vm.runInContext(`
        promise_test(() => new Promise(() => {}), 'stuck body');
        promise_test(async t => t.add_cleanup(() => new Promise(() => {})), 'stuck cleanup');
        promise_test(async () => {}, 'next');
        __wptSeal();
    `, context);
    const results = JSON.parse(JSON.stringify(await context.__wptCompletion));
    assert.deepEqual(results.map(r => [r.name, r.status, r.phase]), [
        ['stuck body', 'TIMEOUT', 'timeout'],
        ['stuck cleanup', 'TIMEOUT', 'cleanup'],
        ['next', 'PASS', 'test'],
    ]);
});
