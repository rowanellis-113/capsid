const results = [];
const pending = new Set();
let promiseQueue = Promise.resolve();
let sealed = false;
let implicitTest = null;
let resolveCompletion;

const completion = new Promise(resolve => {
    resolveCompletion = resolve;
});

class AssertionError extends Error {
    constructor(message) {
        super(message);
        this.name = 'AssertionError';
    }
}

const format = value => {
    if (typeof value === 'string') {
        return JSON.stringify(value);
    }
    try {
        return JSON.stringify(value);
    } catch {
        return String(value);
    }
};

globalThis.format_value = format;

const assertionMessage = (description, detail) =>
    description ? `${description}: ${detail}` : detail;

const maybeComplete = () => {
    if (sealed && pending.size === 0) {
        resolveCompletion(Object.freeze(results.slice()));
    }
};

class HarnessTest {
    constructor(name, asynchronous, promiseTest = false) {
        this.name = name || 'Untitled test';
        this.promiseTest = promiseTest;
        this.completed = false;
        this.cleanups = [];
        if (asynchronous) {
            pending.add(this);
            if (promiseTest) {
                this.finished = new Promise(resolve => { this.resolveFinished = resolve; });
            } else {
                this.startTimeout();
            }
        }
    }

    startTimeout() {
        this.timeout = setTimeout(() => {
            if (this.completed) {
                this.publish('TIMEOUT', 'cleanup timed out', 'TimeoutError', 'cleanup');
            } else {
                this.finish('TIMEOUT', 'test timed out', 'TimeoutError', 'timeout');
            }
        }, 10000);
    }

    step(callback, thisValue, args = []) {
        if (this.completed) {
            return undefined;
        }
        try {
            return callback.apply(thisValue ?? this, args);
        } catch (error) {
            this.fail(error);
            return undefined;
        }
    }

    step_func(callback, thisValue) {
        return (...args) => this.step(callback, thisValue, args);
    }

    step_func_done(callback = () => {}, thisValue) {
        return (...args) => {
            if (this.completed) {
                return;
            }
            this.step(callback, thisValue, args);
            if (!this.completed) {
                this.done();
            }
        };
    }

    step_timeout(callback, delay, ...args) {
        return setTimeout(this.step_func(callback), delay, ...args);
    }

    unreached_func(description = 'reached unreachable code') {
        return this.step_func(() => {
            throw new AssertionError(description);
        });
    }

    add_cleanup(callback) {
        this.cleanups.push(callback);
    }

    fail(error) {
        if (this.completed) {
            return;
        }
        const summary = error?.message ?? String(error);
        const detail = error?.stack
            ? `${summary}\n${error.stack}`
            : summary;
        this.finish('FAIL', detail, error?.name ?? 'Error');
    }

    done() {
        if (!this.completed) {
            this.finish('PASS', '');
        }
    }

    finish(status, message, errorName = null, phase = 'test') {
        this.completed = true;
        const waits = [];
        const cleanupFailed = error => {
            status = 'FAIL';
            phase = 'cleanup';
            errorName = error?.name ?? 'Error';
            const summary = error?.message ?? String(error);
            const detail = error?.stack ? `${summary}\n${error.stack}` : summary;
            message += `${message ? '\n' : ''}cleanup failed: ${detail}`;
        };
        for (const cleanup of this.cleanups) {
            try {
                const value = cleanup();
                if (value === undefined) continue;
                if (this.promiseTest && value && typeof value.then === 'function') {
                    waits.push(Promise.resolve(value).catch(cleanupFailed));
                } else {
                    throw new TypeError('invalid cleanup return value');
                }
            } catch (error) {
                cleanupFailed(error);
            }
        }
        const publish = () => this.publish(status, message, errorName, phase);
        if (waits.length && phase !== 'timeout') {
            Promise.all(waits).then(publish);
        } else {
            publish();
        }
    }

    publish(status, message, errorName, phase) {
        if (this.published) return;
        this.published = true;
        clearTimeout(this.timeout);
        results.push({ name: this.name, status, message, errorName, phase });
        pending.delete(this);
        this.resolveFinished?.();
        maybeComplete();
    }
}

globalThis.setup = options => {
    if (typeof options === 'function') {
        options();
        return;
    }
    if (options?.single_test && !implicitTest) {
        implicitTest = new HarnessTest('single test', true);
    }
};

globalThis.test = (callback, name) => {
    const testCase = new HarnessTest(name, false);
    testCase.step(callback, testCase, [ testCase ]);
    if (!testCase.completed) {
        testCase.done();
    }
    return testCase;
};

globalThis.async_test = (callback, name) => {
    if (typeof callback !== 'function') {
        return new HarnessTest(callback, true);
    }
    const testCase = new HarnessTest(name, true);
    testCase.step(callback, testCase, [ testCase ]);
    return testCase;
};

globalThis.promise_test = (callback, name) => {
    const testCase = new HarnessTest(name, true, true);
    promiseQueue = promiseQueue.then(() => {
        testCase.startTimeout();
        try {
            const promise = callback.call(testCase, testCase);
            if (!promise || typeof promise.then !== 'function') {
                throw new AssertionError('promise_test callback must return a thenable');
            }
            Promise.resolve(promise).then(() => testCase.done(), error => testCase.fail(error));
        } catch (error) {
            testCase.fail(error);
        }
        return testCase.finished;
    });
    return testCase;
};

globalThis.step_timeout = (callback, delay, ...args) =>
    setTimeout(callback, delay, ...args);

globalThis.done = () => {
    if (implicitTest) {
        implicitTest.done();
        return;
    }
    sealed = true;
    maybeComplete();
};

globalThis.assert_true = (actual, description) => {
    if (actual !== true) {
        throw new AssertionError(
            assertionMessage(description, `expected true, got ${format(actual)}`),
        );
    }
};

globalThis.assert_false = (actual, description) => {
    if (actual !== false) {
        throw new AssertionError(
            assertionMessage(description, `expected false, got ${format(actual)}`),
        );
    }
};

globalThis.assert_equals = (actual, expected, description) => {
    if (!Object.is(actual, expected)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${format(expected)}, got ${format(actual)}`,
        ));
    }
};

globalThis.assert_not_equals = (actual, expected, description) => {
    if (Object.is(actual, expected)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected values to differ, both were ${format(actual)}`,
        ));
    }
};

globalThis.assert_array_equals = (actual, expected, description) => {
    if (typeof actual !== 'object' || actual === null ||
        !('length' in actual) || expected == null ||
        actual.length !== expected.length) {
        throw new AssertionError(assertionMessage(
            description,
            `array lengths differ: ${format(actual)} vs ${format(expected)}`,
        ));
    }
    for (let index = 0; index < expected.length; ++index) {
        if (Object.prototype.hasOwnProperty.call(actual, index) !==
                Object.prototype.hasOwnProperty.call(expected, index) ||
            !Object.is(actual[index], expected[index])) {
            throw new AssertionError(assertionMessage(
                description,
                `arrays differ at ${index}: ` +
                    `${format(actual[index])} vs ${format(expected[index])}`,
            ));
        }
    }
};

globalThis.assert_own_property = (object, property, description) => {
    if (!Object.prototype.hasOwnProperty.call(object, property)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected own property ${format(property)}`,
        ));
    }
};

globalThis.assert_inherits = (object, property, description) => {
    if (!(property in object) ||
        Object.prototype.hasOwnProperty.call(object, property)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected inherited property ${format(property)}`,
        ));
    }
};

globalThis.assert_in_array = (actual, expected, description) => {
    if (expected.indexOf(actual) === -1) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${format(actual)} to be in ${format(expected)}`,
        ));
    }
};

globalThis.assert_regexp_match = (actual, expected, description) => {
    if (!expected.test(actual)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${format(actual)} to match ${expected}`,
        ));
    }
};

globalThis.assert_class_string = (object, expected, description) => {
    const actual = Object.prototype.toString.call(object);
    const classString = `[object ${expected}]`;
    if (actual !== classString) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${classString}, got ${actual}`,
        ));
    }
};

globalThis.assert_approx_equals =
    (actual, expected, epsilon, description) => {
        const matches = typeof actual === 'number' &&
            (isFinite(actual) || isFinite(expected)
                ? Math.abs(actual - expected) <= epsilon
                : Object.is(actual, expected));
        if (!matches) {
            throw new AssertionError(assertionMessage(
                description,
                `expected ${format(actual)} to be within ${epsilon} of ` +
                    format(expected),
            ));
        }
    };

globalThis.assert_greater_than = (actual, expected, description) => {
    if ((typeof actual !== 'number' && typeof actual !== 'bigint') ||
        typeof actual !== typeof expected || !(actual > expected)) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${format(actual)} to be greater than ${format(expected)}`,
        ));
    }
};

globalThis.assert_throws_js = (constructor, callback, description) => {
    let thrown = false;
    try {
        callback();
    } catch (error) {
        thrown = true;
        assertErrorConstructor(error, constructor, description);
    }
    if (!thrown) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${constructor?.name} to be thrown`,
        ));
    }
};

const assertErrorConstructor = (error, constructor, description) => {
    if (error instanceof AssertionError) {
        throw error;
    }
    let ancestor = constructor;
    while (ancestor &&
           !(typeof ancestor === 'function' && ancestor.name === 'Error')) {
        ancestor = Object.getPrototypeOf(ancestor);
    }
    if (typeof constructor !== 'function' || !ancestor ||
        typeof error !== 'object' || error === null ||
        error.constructor !== constructor || error.name !== constructor.name) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${constructor?.name}, got ${error?.constructor?.name}`,
        ));
    }
};

globalThis.assert_throws_exactly = (expected, callback, description) => {
    let thrown = false;
    try {
        callback();
    } catch (error) {
        thrown = true;
        if (error instanceof AssertionError) {
            throw error;
        }
        if (!Object.is(error, expected)) {
            throw new AssertionError(assertionMessage(
                description,
                `expected ${format(expected)}, got ${format(error)}`,
            ));
        }
    }
    if (!thrown) {
        throw new AssertionError(assertionMessage(
            description,
            `expected ${format(expected)} to be thrown`,
        ));
    }
};

globalThis.assert_throws_dom =
    (name, constructorOrCallback, callbackOrDescription, maybeDescription) => {
        const hasConstructor =
            constructorOrCallback?.name === 'DOMException';
        const constructor =
            hasConstructor ? constructorOrCallback : DOMException;
        const callback =
            hasConstructor ? callbackOrDescription : constructorOrCallback;
        const description =
            hasConstructor ? maybeDescription : callbackOrDescription;
        let error;
        try {
            callback();
        } catch (caught) {
            error = caught;
        }
        if (!(error instanceof constructor) || error.name !== name) {
            throw new AssertionError(assertionMessage(
                description,
                `expected ${name}, got ${error?.name ?? 'no exception'}`,
            ));
        }
    };

globalThis.assert_throws_quotaexceedederror =
    (...args) => {
        const explicit = args[0]?.name === 'QuotaExceededError';
        const constructor = explicit ? args.shift() : globalThis.QuotaExceededError;
        const [callback, requested, quota, description] = args;
        const check = (ok, detail) => {
            if (!ok) throw new AssertionError(assertionMessage(description, detail));
        };
        check(args.length <= 4, 'unexpected extra argument');
        let error;
        try {
            callback();
        } catch (caught) {
            if (caught instanceof AssertionError) throw caught;
            error = caught;
        }
        check(error !== null && typeof error === 'object', 'expected a thrown exception object');
        check(error.name === 'QuotaExceededError' && error.code == 22, 'incorrect exception name or code');
        for (const [key, expected] of Object.entries({ requested, quota })) {
            if (typeof expected === 'function') {
                check(expected(error[key]), 'failed predicate for ' + key);
            } else {
                check(expected === null || typeof expected === 'number', 'invalid expectation for ' + key);
                check(key in error && error[key] == expected, 'incorrect or missing ' + key);
            }
        }
        check(error.constructor === constructor, 'wrong QuotaExceededError constructor');
    };

globalThis.promise_rejects_js =
    async (_test, constructor, promise, description) => {
        try {
            await promise;
        } catch (error) {
            assertErrorConstructor(error, constructor, description);
            return error;
        }
        throw new AssertionError(assertionMessage(
            description,
            `expected rejection with ${constructor.name}`,
        ));
    };

globalThis.promise_rejects_exactly =
    async (_test, expected, promise, description) => {
        try {
            await promise;
        } catch (error) {
            if (error instanceof AssertionError) {
                throw error;
            }
            if (Object.is(error, expected)) {
                return error;
            }
            throw new AssertionError(assertionMessage(
                description,
                `expected rejection with ${format(expected)}, got ` +
                    format(error),
            ));
        }
        throw new AssertionError(assertionMessage(
            description,
            `expected rejection with ${format(expected)}`,
        ));
    };

globalThis.assert_unreached = (description = 'reached unreachable code') => {
    throw new AssertionError(description);
};

globalThis.__wptSeal = () => {
    sealed = true;
    maybeComplete();
};

globalThis.__wptCompletion = completion;
