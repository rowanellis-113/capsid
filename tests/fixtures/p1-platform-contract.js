const failures = [];

function check(condition, message) {
    if (!condition) {
        failures.push(message);
    }
}

async function testEventsAndReporting() {
    check('onerror' in globalThis, 'globalThis.onerror');
    check('onunhandledrejection' in globalThis,
        'globalThis.onunhandledrejection');
    check('onrejectionhandled' in globalThis,
        'globalThis.onrejectionhandled');

    const target = new EventTarget();
    const order = [];
    const objectListener = {
        handleEvent(event) {
            order.push(`object:${event.type}`);
        },
    };
    target.addEventListener('capsid', () => order.push('first'), { once: true });
    target.addEventListener('capsid', objectListener);
    const first = new CustomEvent('capsid', {
        cancelable: true,
        detail: 42,
    });
    target.addEventListener('capsid', event => event.preventDefault());
    check(target.dispatchEvent(first) === false, 'cancelable event result');
    target.dispatchEvent(new Event('capsid'));
    check(
        order.join(',') === 'first,object:capsid,object:capsid',
        `event listener order: ${order.join(',')}`,
    );
    check(first.detail === 42, 'CustomEvent.detail');

    const constructorPromise = Promise.resolve();
    // The pinned workerd profile creates these events internally. Public calls
    // and construction reject before processing the event dictionary.
    for (const [name, construct] of [
        ['new', (...args) => new PromiseRejectionEvent(...args)],
        ['call', (...args) => PromiseRejectionEvent(...args)],
    ]) {
        let dictionaryReads = 0;
        let rejected = false;
        try {
            construct('contract', {
                get promise() { dictionaryReads++; return constructorPromise; },
                get reason() { dictionaryReads++; return 42; },
                get cancelable() { dictionaryReads++; return true; },
            });
        } catch (error) {
            rejected = error instanceof TypeError;
        }
        check(rejected, `PromiseRejectionEvent ${name} rejects (workerd profile)`);
        check(dictionaryReads === 0, `PromiseRejectionEvent ${name} leaves dictionary untouched`);
    }
    let missingPromiseThrew = false;
    try {
        new PromiseRejectionEvent('contract', {});
    } catch (error) {
        missingPromiseThrew = error instanceof TypeError;
    }
    check(missingPromiseThrew, 'PromiseRejectionEvent rejects an empty dictionary');

    let reported;
    const onError = event => {
        reported = event;
        event.preventDefault();
    };
    addEventListener('error', onError, { once: true });
    const reportedError = new Error('reported-by-contract');
    reportError(reportedError);
    check(reported instanceof ErrorEvent, 'reportError ErrorEvent');
    check(reported?.message === 'Uncaught Error: reported-by-contract',
        'reportError message');
    check(reported?.filename === 'capsid:app/main',
        'reportError source filename');
    check(reported?.lineno > 0 && reported?.colno > 0,
        'reportError source coordinates');
    check(reported?.error === reportedError, 'reportError error identity');
    let missingReportErrorArgumentThrew = false;
    try {
        reportError();
    } catch (error) {
        missingReportErrorArgumentThrew = error instanceof TypeError;
    }
    check(missingReportErrorArgumentThrew, 'reportError requires an argument');

    let getterInvoked = false;
    const opaqueError = {
        get name() {
            getterInvoked = true;
        },
        get message() {
            getterInvoked = true;
        },
        get fileName() {
            getterInvoked = true;
        },
        get lineNumber() {
            getterInvoked = true;
        },
    };
    addEventListener('error', event => event.preventDefault(), { once: true });
    reportError(opaqueError);
    check(!getterInvoked, 'reportError must not invoke error getters');

    const throwing = new EventTarget();
    const listenerError = new TypeError('listener-failure');
    const event = new Event('explode'); let caught;
    throwing.addEventListener('explode', () => { throw listenerError; });
    try { throwing.dispatchEvent(event); } catch (error) { caught = error; }
    check(caught === listenerError, 'listener exception identity (workerd default profile)');
    check(event.eventPhase === 0, 'dispatch phase reset after listener exception');

    let rejectionEvent;
    const onUnhandled = event => {
        rejectionEvent = event;
        event.preventDefault();
    };
    addEventListener('unhandledrejection', onUnhandled, { once: true });
    const reason = new Error('unhandled-by-contract');
    const rejectedPromise = Promise.reject(reason);
    await new Promise(resolve => setTimeout(resolve, 0));
    check(
        rejectionEvent instanceof PromiseRejectionEvent,
        'unhandledrejection event',
    );
    check(rejectionEvent?.reason === reason, 'unhandledrejection reason');
    check(rejectionEvent?.promise === rejectedPromise, 'unhandledrejection exact promise identity');
    check(rejectionEvent?.promise instanceof Promise,
        'unhandledrejection promise');

    let handledEvent;
    addEventListener('rejectionhandled', event => {
        handledEvent = event;
    }, { once: true });
    rejectionEvent?.promise.catch(() => {});
    await new Promise(resolve => setTimeout(resolve, 0));
    check(
        handledEvent instanceof PromiseRejectionEvent,
        'rejectionhandled event',
    );
    check(handledEvent?.promise === rejectionEvent?.promise,
        'rejectionhandled promise');
    // Pinned workerd omits reason on rejectionhandled; unhandledrejection above
    // still checks the original reason identity. Both are differential-tested.
    check(handledEvent?.reason === undefined, 'rejectionhandled reason (workerd profile)');
}

async function testTimers() {
    const order = [];
    const canceled = setTimeout(() => order.push('canceled'), 0);
    clearTimeout(canceled);
    queueMicrotask(() => order.push('microtask'));
    await new Promise(resolve => {
        setTimeout(() => {
            order.push('timeout');
            resolve();
        }, 0);
    });
    check(order.join(',') === 'microtask,timeout',
        `timer/microtask order: ${order.join(',')}`);

    let ticks = 0;
    await new Promise(resolve => {
        const interval = setInterval(() => {
            ticks += 1;
            if (ticks === 2) {
                clearInterval(interval);
                resolve();
            }
        }, 0);
    });
    await new Promise(resolve => setTimeout(resolve, 1));
    check(ticks === 2, `clearInterval: ${ticks}`);
}

async function testEncodingAndUrl() {
    const encoder = new TextEncoder();
    const bytes = encoder.encode('A😀');
    check(
        Array.from(bytes).join(',') === '65,240,159,152,128',
        'TextEncoder UTF-8',
    );
    const destination = new Uint8Array(5);
    const encoded = encoder.encodeInto('éx', destination);
    check(encoded.read === 2 && encoded.written === 3,
        'TextEncoder.encodeInto result');
    check(new TextDecoder().decode(destination.subarray(0, 3)) === 'éx',
        'TextDecoder round trip');
    let fatalThrew = false;
    try {
        new TextDecoder('utf-8', { fatal: true }).decode(
            new Uint8Array([ 0xff ]),
        );
    } catch (error) {
        fatalThrew = error instanceof TypeError;
    }
    check(fatalThrew, 'TextDecoder fatal error');
    const latin2 = new TextDecoder(' \tISO-8859-2\r\n');
    check(latin2.encoding === 'iso-8859-2',
        'TextDecoder single-byte label normalization');
    check(latin2.decode(new Uint8Array([ 0x41, 0xA1 ])) === 'AĄ',
        'TextDecoder single-byte index');
    check(
        new TextDecoder('windows-1252').decode(
            new Uint8Array([ 0x80, 0x81 ])) === '€\u0081',
        'TextDecoder windows-1252 undefined compatibility code point');
    const gbStream = new TextDecoder('gb18030');
    check(
        gbStream.decode(
            new Uint8Array([ 0x41, 0x81 ]), { stream: true }) === 'A',
        'TextDecoder GB18030 stream emits complete prefix');
    check(
        gbStream.decode(
            new Uint8Array([ 0x35, 0xF4 ]), { stream: true }) === '',
        'TextDecoder GB18030 stream retains partial sequence');
    check(
        gbStream.decode(
            new Uint8Array([ 0x37 ]), { stream: true }) === '\uE7C7',
        'TextDecoder GB18030 stream emits completed sequence');
    check(gbStream.decode() === '',
        'TextDecoder GB18030 stream clean EOF');

    const gbEof = new TextDecoder('gbk');
    check(
        gbEof.decode(
            new Uint8Array([ 0x81 ]), { stream: true }) === '',
        'TextDecoder GBK retains lead byte');
    check(gbEof.decode() === '\uFFFD',
        'TextDecoder GBK emits replacement at EOF');
    check(gbEof.decode(new Uint8Array([ 0x41 ])) === 'A',
        'TextDecoder GBK resets after EOF');

    const gbFatal = new TextDecoder('gb18030', { fatal: true });
    let gbFatalThrew = false;
    try {
        gbFatal.decode(new Uint8Array([ 0x81, 0xFF ]));
    } catch (error) {
        gbFatalThrew = error instanceof TypeError;
    }
    check(gbFatalThrew, 'TextDecoder GB18030 fatal error');
    check(gbFatal.decode(new Uint8Array([ 0x41 ])) === 'A',
        'TextDecoder GB18030 resets after fatal error');

    const url = new URL('../c?x=1&x=2#d', 'https://example.test/a/b');
    check(url.href === 'https://example.test/c?x=1&x=2#d', 'URL resolution');
    check(url.searchParams.getAll('x').join(',') === '1,2',
        'URLSearchParams duplicate values');
    url.searchParams.append('space', 'a b');
    check(url.search.includes('space=a+b'), 'URLSearchParams encoding');
    const pattern = new URLPattern({ pathname: '/users/:id' });
    check(pattern.exec('https://example.test/users/42')?.pathname.groups.id === '42',
        'URLPattern named group');
}

async function testStreamsAndBodies() {
    const source = new ReadableStream({
        start(controller) {
            controller.enqueue(new Uint8Array([ 1, 2 ]));
            controller.enqueue(new Uint8Array([ 3 ]));
            controller.close();
        },
    });
    const transform = new TransformStream({
        transform(chunk, controller) {
            controller.enqueue(Uint8Array.from(chunk, value => value + 1));
        },
    });
    const reader = source.pipeThrough(transform).getReader();
    const output = [];
    for (;;) {
        const result = await reader.read();
        if (result.done) {
            break;
        }
        output.push(...result.value);
    }
    check(output.join(',') === '2,3,4', 'TransformStream pipeline');

    const blob = new Blob([ 'capsid', new Uint8Array([ 33 ]) ], {
        type: 'Text/Plain',
    });
    check(blob.size === 7 && blob.type === 'text/plain', 'Blob metadata');
    check(await blob.text() === 'capsid!', 'Blob.text');
    const file = new File([ blob ], 'capsid.txt', {
        type: 'text/plain',
        lastModified: 123,
    });
    check(
        file.name === 'capsid.txt' && file.lastModified === 123 &&
            await file.text() === 'capsid!',
        'File behavior',
    );

    const form = new FormData();
    form.append('name', 'capsid');
    form.append('file', file);
    check(form.get('name') === 'capsid', 'FormData string entry');
    check(form.get('file') instanceof File, 'FormData file entry');

    const multipartPrefix = new TextEncoder().encode(
        '--capsid-boundary\r\n' +
        'Content-Disposition: form-data; name="upload"; ' +
        'filename="binary.bin"\r\n' +
        'Content-Type: application/octet-stream\r\n\r\n',
    );
    const multipartFileBytes = new Uint8Array([ 0x00, 0xff, 0x80, 0x41 ]);
    const multipartSuffix =
        new TextEncoder().encode('\r\n--capsid-boundary--\r\n');
    const multipartBody = new Uint8Array(
        multipartPrefix.length +
        multipartFileBytes.length +
        multipartSuffix.length,
    );
    multipartBody.set(multipartPrefix, 0);
    multipartBody.set(multipartFileBytes, multipartPrefix.length);
    multipartBody.set(
        multipartSuffix,
        multipartPrefix.length + multipartFileBytes.length,
    );
    const multipartRequest = new Request('https://example.test/upload', {
        method: 'POST',
        headers: {
            'content-type':
                'multipart/form-data; boundary=capsid-boundary',
        },
        body: multipartBody,
    });
    const parsedMultipart = await multipartRequest.formData();
    const parsedFile = parsedMultipart.get('upload');
    check(
        parsedFile !== null &&
            typeof parsedFile.arrayBuffer === 'function',
        'multipart binary file entry',
    );
    if (parsedFile !== null &&
        typeof parsedFile.arrayBuffer === 'function') {
        const parsedBytes =
            new Uint8Array(await parsedFile.arrayBuffer());
        check(
            parsedBytes.length === multipartFileBytes.length &&
                parsedBytes.every(
                    (value, index) =>
                        value === multipartFileBytes[index]),
            `multipart binary bytes: ${Array.from(parsedBytes).join(',')}`,
        );
    }
    check(multipartRequest.bodyUsed,
        'Request.formData marks the body as used');
    let multipartSecondReadRejected = false;
    try {
        await multipartRequest.arrayBuffer();
    } catch (error) {
        multipartSecondReadRejected = error instanceof TypeError;
    }
    check(multipartSecondReadRejected,
        'Request.formData prevents a second body read');

    const multipartResponse = new Response(multipartBody.slice(), {
        headers: {
            'content-type':
                'multipart/form-data; boundary=capsid-boundary',
        },
    });
    const responseMultipart = await multipartResponse.formData();
    const responseMultipartFile = responseMultipart.get('upload');
    check(
        responseMultipartFile !== null &&
            typeof responseMultipartFile.arrayBuffer === 'function',
        'Response multipart binary file entry',
    );
    if (responseMultipartFile !== null &&
        typeof responseMultipartFile.arrayBuffer === 'function') {
        const responseMultipartBytes =
            new Uint8Array(await responseMultipartFile.arrayBuffer());
        check(
            responseMultipartBytes.length === multipartFileBytes.length &&
                responseMultipartBytes.every(
                    (value, index) =>
                        value === multipartFileBytes[index]),
            'Response multipart preserves binary bytes: ' +
                `${Array.from(responseMultipartBytes).join(',')}`,
        );
    }

    const mixedCaseMultipartRequest =
        new Request('https://example.test/upload', {
            method: 'POST',
            headers: {
                'content-type':
                    'Multipart/Form-Data; Boundary="capsid-boundary"',
            },
            body: multipartBody.slice(),
        });
    const mixedCaseMultipart =
        await mixedCaseMultipartRequest.formData();
    const mixedCaseMultipartFile = mixedCaseMultipart.get('upload');
    check(
        mixedCaseMultipartFile !== null &&
            typeof mixedCaseMultipartFile.arrayBuffer === 'function',
        'multipart media type and boundary parameter are case-insensitive',
    );
    if (mixedCaseMultipartFile !== null &&
        typeof mixedCaseMultipartFile.arrayBuffer === 'function') {
        const mixedCaseMultipartBytes =
            new Uint8Array(await mixedCaseMultipartFile.arrayBuffer());
        check(
            mixedCaseMultipartBytes.length === multipartFileBytes.length &&
                mixedCaseMultipartBytes.every(
                    (value, index) =>
                        value === multipartFileBytes[index]),
            'mixed-case multipart preserves binary bytes: ' +
                `${Array.from(mixedCaseMultipartBytes).join(',')}`,
        );
    }

    /*
     * A boundary token inside a file is ordinary payload unless it appears as
     * a complete delimiter line. Searching for the token alone truncates valid
     * binary data.
     */
    const collisionFileBytes = new Uint8Array([
        0x41, 0x2d, 0x2d, 0x77, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x2d,
        0x62, 0x6f, 0x75, 0x6e, 0x64, 0x61, 0x72, 0x79, 0x2d, 0x58,
        0xff,
    ]);
    const collisionBody = new Uint8Array(
        multipartPrefix.length +
        collisionFileBytes.length +
        multipartSuffix.length,
    );
    collisionBody.set(multipartPrefix, 0);
    collisionBody.set(collisionFileBytes, multipartPrefix.length);
    collisionBody.set(
        multipartSuffix,
        multipartPrefix.length + collisionFileBytes.length,
    );
    const collisionRequest =
        new Request('https://example.test/upload', {
            method: 'POST',
            headers: {
                'content-type':
                    'multipart/form-data; boundary=capsid-boundary',
            },
            body: collisionBody,
        });
    const collisionMultipart = await collisionRequest.formData();
    const collisionFile = collisionMultipart.get('upload');
    check(
        collisionFile !== null &&
            typeof collisionFile.arrayBuffer === 'function',
        'multipart file containing a boundary-like byte sequence',
    );
    if (collisionFile !== null &&
        typeof collisionFile.arrayBuffer === 'function') {
        const collisionParsedBytes =
            new Uint8Array(await collisionFile.arrayBuffer());
        check(
            collisionParsedBytes.length === collisionFileBytes.length &&
                collisionParsedBytes.every(
                    (value, index) =>
                        value === collisionFileBytes[index]),
            'multipart ignores boundary-like bytes inside file payload: ' +
                `${Array.from(collisionParsedBytes).join(',')}`,
        );
    }

    const response = new Response(blob, {
        status: 201,
        headers: { 'x-capsid': 'yes' },
    });
    const clone = response.clone();
    check(response.status === 201 && response.headers.get('x-capsid') === 'yes',
        'Response metadata');
    check(await response.text() === 'capsid!' && await clone.text() === 'capsid!',
        'Response clone body');
    check(clone instanceof Response, 'Response.clone result brand');
    check(clone.headers instanceof Headers, 'Response.clone Headers brand');

    try {
        const errorClone = Response.error().clone();
        check(errorClone instanceof Response,
            'Response.error clone result brand');
        check(
            errorClone.status === 0 &&
                errorClone.type === 'error' &&
                !errorClone.ok,
            'Response.error clone metadata',
        );
        check(errorClone.headers instanceof Headers,
            'Response.error clone Headers brand');
    } catch (error) {
        failures.push(
            'Response.error clone must not throw:' +
            `${error?.constructor?.name ?? error}`,
        );
    }

    const request = new Request('https://example.test/clone', {
        method: 'POST',
        headers: { 'x-capsid': 'yes' },
        body: 'request-body',
    });
    const requestClone = request.clone();
    check(requestClone instanceof Request, 'Request.clone result brand');
    check(requestClone.headers instanceof Headers,
        'Request.clone Headers brand');
    check(
        await request.text() === 'request-body' &&
            await requestClone.text() === 'request-body',
        'Request clone body',
    );
}

async function testCompressionAndCrypto() {
    const input = new TextEncoder().encode('capsid-'.repeat(128));
    const compressed = new Blob([ input ])
        .stream()
        .pipeThrough(new CompressionStream('gzip'));
    const decompressed = compressed.pipeThrough(
        new DecompressionStream('gzip'),
    );
    const output = new Uint8Array(await new Response(decompressed).arrayBuffer());
    check(output.length === input.length, 'compression length');
    check(output.every((value, index) => value === input[index]),
        'compression round trip');

    const digest = new Uint8Array(
        await crypto.subtle.digest(
            'SHA-256',
            new TextEncoder().encode('abc'),
        ),
    );
    const hex = Array.from(digest, value => value.toString(16).padStart(2, '0'))
        .join('');
    check(
        hex === 'ba7816bf8f01cfea414140de5dae2223' +
            'b00361a396177a9cb410ff61f20015ad',
        'SubtleCrypto.digest',
    );
    check(
        /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/
            .test(crypto.randomUUID()),
        'crypto.randomUUID v4',
    );
    const random = new Uint8Array(32);
    crypto.getRandomValues(random);
    check(random.some(value => value !== 0), 'crypto.getRandomValues');

    const quotaErr = new QuotaExceededError('x', { quota: 10, requested: 20 });
    const clonedQuota = structuredClone(quotaErr);
    check(
        clonedQuota !== quotaErr &&
            clonedQuota.constructor === QuotaExceededError &&
            clonedQuota.name === 'QuotaExceededError' &&
            clonedQuota.message === 'x' &&
            clonedQuota.code === 22 &&
            clonedQuota.quota === 10 &&
            clonedQuota.requested === 20,
        'structuredClone preserves QuotaExceededError top-level',
    );
    const defaultQuota = structuredClone(new QuotaExceededError('d'));
    check(
        defaultQuota.constructor === QuotaExceededError &&
            defaultQuota.quota === null &&
            defaultQuota.requested === null,
        'structuredClone preserves default null QuotaExceededError fields',
    );
    const graph = { a: quotaErr, b: quotaErr, list: [quotaErr] };
    graph.self = graph;
    const clonedGraph = structuredClone(graph);
    check(
        clonedGraph.self === clonedGraph &&
            clonedGraph.a === clonedGraph.b &&
            clonedGraph.a === clonedGraph.list[0] &&
            clonedGraph.a !== quotaErr &&
            clonedGraph.a.constructor === QuotaExceededError &&
            clonedGraph.a.quota === 10 &&
            clonedGraph.a.requested === 20,
        'structuredClone preserves nested, cyclic and shared QuotaExceededError refs',
    );
    check(
        Object.is(new QuotaExceededError('', { quota: -0 }).quota, -0) &&
            Object.is(structuredClone(new QuotaExceededError('', { quota: -0 })).quota, -0),
        'QuotaExceededError preserves signed negative zero',
    );
    for (const [label, fn, expectedName] of [
        ['missing arg', () => crypto.getRandomValues(), 'TypeError'],
        ['plain object', () => crypto.getRandomValues({}), 'TypeError'],
        ['Float32Array', () => crypto.getRandomValues(new Float32Array(1)), 'TypeMismatchError'],
        ['DataView', () => crypto.getRandomValues(new DataView(new ArrayBuffer(4))), 'TypeMismatchError'],
    ]) {
        let actualName = 'no-throw';
        try {
            fn();
        } catch (err) {
            actualName = err?.name ?? String(err);
        }
        check(
            actualName === expectedName,
            `crypto.getRandomValues(${label}) expected ${expectedName}, got ${actualName}`,
        );
    }
    const shadowedView = new Uint8Array(1);
    Object.defineProperty(shadowedView, 'byteLength', { value: 65537 });
    check(
        crypto.getRandomValues(shadowedView) === shadowedView,
        'crypto.getRandomValues uses internal [[ByteLength]] slot',
    );
    let oversizedOk = false;
    try {
        crypto.getRandomValues(new Uint8Array(65537));
    } catch (err) {
        oversizedOk =
            err.constructor === QuotaExceededError &&
            err.name === 'QuotaExceededError' &&
            err.code === 22 &&
            err.quota === null &&
            err.requested === null;
    }
    check(oversizedOk, 'crypto.getRandomValues oversized throws QuotaExceededError');
}

function checkCase(label, run) {
    try {
        check(run() === true, label);
    } catch (error) {
        failures.push(`${label}: threw ${error.name} (${error.message})`);
    }
}

function testQuotaCloneAndTransfer() {
    const quota = new QuotaExceededError('limit', { quota: 10, requested: 20 });
    function isClonedQuota(value) {
        return value !== quota && value instanceof QuotaExceededError &&
            value.message === 'limit' && value.quota === 10 && value.requested === 20;
    }
    function throwsDataCloneError(run) {
        try {
            run();
        } catch (error) {
            return error instanceof DOMException && error.name === 'DataCloneError';
        }
        return false;
    }
    checkCase('clone keeps enumerable array properties and aliases', () => {
        const source = [quota];
        source.detail = quota;
        const copy = structuredClone(source);
        return Array.isArray(copy) && Object.hasOwn(copy, 'detail') &&
            isClonedQuota(copy[0]) && copy.detail === copy[0];
    });
    checkCase('clone preserves sparse array holes and trailing length', () => {
        const source = new Array(5);
        source[2] = quota;
        const copy = structuredClone(source);
        return Array.isArray(copy) && copy.length === 5 &&
            Object.keys(copy).join(',') === '2' && isClonedQuota(copy[2]);
    });
    checkCase('clone copies own data on a class instance containing a quota error', () => {
        class Record {
            constructor() {
                this.error = quota;
                this.label = 'batch';
            }
        }
        const copy = structuredClone(new Record());
        return Object.getPrototypeOf(copy) === Object.prototype && copy.label === 'batch' &&
            Object.hasOwn(copy, 'error') && isClonedQuota(copy.error);
    });
    checkCase('clone rejects duplicate transfers before detaching the buffer', () => {
        const bytes = Uint8Array.of(3, 17, 5);
        const buffer = bytes.buffer;
        const rejected = throwsDataCloneError(() =>
            structuredClone(quota, { transfer: [buffer, buffer] }));
        return rejected && buffer.byteLength === 3 && bytes.join(',') === '3,17,5';
    });
    checkCase('clone rejects an already detached transfer', () => {
        const buffer = new ArrayBuffer(3);
        const first = structuredClone(buffer, { transfer: [buffer] });
        return first.byteLength === 3 && buffer.byteLength === 0 &&
            throwsDataCloneError(() => structuredClone(quota, { transfer: [buffer] }));
    });
    checkCase('clone reads transfer once and preserves transferred buffer identity and bytes', () => {
        const buffer = Uint8Array.of(3, 17, 5).buffer;
        let reads = 0;
        const copy = structuredClone({ error: quota, buffer, alias: buffer }, {
            get transfer() {
                ++reads;
                return [buffer];
            },
        });
        return reads === 1 && buffer.byteLength === 0 && isClonedQuota(copy.error) &&
            copy.buffer !== buffer && copy.buffer === copy.alias && copy.buffer.byteLength === 3 &&
            new Uint8Array(copy.buffer).join(',') === '3,17,5';
    });
    checkCase('transfer snapshots bytes after getters and preserves both view backings', () => {
        const data = Uint8Array.of(4, 8, 12, 16);
        const view = new DataView(data.buffer, 1, 2);
        const source = { data, view, buffer: data.buffer, get last() {
            data[1] = 51;
            return quota;
        } };
        const copy = structuredClone(source, { transfer: [data.buffer] });
        return data.buffer.byteLength === 0 && copy.buffer === copy.data.buffer &&
            copy.buffer === copy.view.buffer && copy.view.byteOffset === 1 &&
            copy.view.byteLength === 2 && copy.view.getUint8(0) === 51 &&
            copy.data.join(',') === '4,51,12,16' && isClonedQuota(copy.last);
    });
    checkCase('clone propagates the original getter exception before transferring', () => {
        const buffer = Uint8Array.of(29).buffer;
        const failure = new RangeError('stop');
        let calls = 0;
        try {
            structuredClone({ error: quota, get next() { ++calls; throw failure; } },
                { transfer: [buffer] });
        } catch (error) {
            return error === failure && calls === 1 && new Uint8Array(buffer)[0] === 29;
        }
        return false;
    });
}

function testRandomValueViews() {
    checkCase('getRandomValues returns an attached empty integer view', () => {
        const view = new Uint8Array(0);
        return crypto.getRandomValues(view) === view;
    });
    for (const length of [0, 1]) {
        checkCase(`getRandomValues returns the detached integer view (original length ${length})`, () => {
            const buffer = new ArrayBuffer(length);
            const view = new Uint8Array(buffer);
            const transferred = structuredClone(buffer, { transfer: [buffer] });
            // Even a buffer originally of length zero must actually have detached.
            let detached = false;
            try {
                new Uint8Array(buffer);
            } catch (error) {
                detached = error instanceof TypeError;
            }
            return detached && transferred.byteLength === length && view.byteLength === 0 &&
                crypto.getRandomValues(view) === view;
        });
    }
    checkCase('getRandomValues rejects a shared integer view with TypeError', () => {
        const view = new Uint8Array(new SharedArrayBuffer(1));
        try {
            crypto.getRandomValues(view);
        } catch (error) {
            return error instanceof TypeError;
        }
        return false;
    });
}

function testConsoleAndPerformance() {
    const required = [
        'assert', 'clear', 'count', 'countReset', 'debug', 'dir', 'dirxml',
        'error', 'group', 'groupCollapsed', 'groupEnd', 'info', 'log', 'table',
        'time', 'timeEnd', 'timeLog', 'trace', 'warn',
    ];
    check(
        required.every(name => typeof console[name] === 'function'),
        'Console Standard method surface',
    );

    for (const operation of [
        () => console.count('contract'),
        () => console.countReset('contract'),
        () => console.group('contract'),
        () => console.groupEnd(),
        () => console.time('contract'),
        () => console.timeLog('contract'),
        () => console.timeEnd('contract'),
        () => console.dir({ contract: true }),
        () => console.table([ { contract: true } ]),
        () => console.trace('contract'),
        () => console.clear(),
    ]) {
        try {
            operation();
        } catch (error) {
            failures.push(`console operation threw: ${error}`);
        }
    }

    let illegalConstructor = false;
    try {
        new Performance();
    } catch (error) {
        illegalConstructor = error instanceof TypeError;
    }
    check(illegalConstructor, 'Performance illegal constructor');
    const first = performance.now();
    const second = performance.now();
    check(Number.isFinite(performance.timeOrigin) && performance.timeOrigin > 0,
        'Performance.timeOrigin');
    check(Number.isFinite(first) && second >= first, 'Performance.now monotonic');
    check(performance instanceof Performance, 'Performance brand');
    check(performance.toJSON().timeOrigin === performance.timeOrigin,
        'Performance.toJSON');
}

export default {
    async fetch() {
        await testEventsAndReporting();
        await testTimers();
        await testEncodingAndUrl();
        await testStreamsAndBodies();
        await testCompressionAndCrypto();
        testQuotaCloneAndTransfer();
        testRandomValueViews();
        testConsoleAndPerformance();
        return Response.json({
            profile: 'CAPSID-MIN-2025-subset-v0',
            passed: failures.length === 0,
            failures,
        });
    },
};
