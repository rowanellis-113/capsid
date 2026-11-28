import telemetry from 'capsid:binding/telemetry';
import vault from 'capsid:binding/vault';

// Untrusted User Runtime application exercising every Binding RPC v2 (v0.4.0)
// capability across clean HTTP endpoints.

const encoder = new TextEncoder();

class CustomRequest extends Request {
  constructor(url, init) {
    super(url, init);
    this.customTag = 'subclass-ok';
  }
}

let releaseFenceGate = null;

function isInvalidStateError(err) {
  return err instanceof DOMException && err.name === 'InvalidStateError';
}

async function expectAsyncInvalidState(fn) {
  try {
    await fn();
    return false;
  } catch (err) {
    return isInvalidStateError(err);
  }
}

function expectSyncInvalidState(fn) {
  try {
    fn();
    return false;
  } catch (err) {
    return isInvalidStateError(err);
  }
}

async function handleM4aFairScheduling() {
  const order = [];
  const startedAt = performance.now();
  const calls = [];
  for (let i = 0; i < 20; ++i) {
    calls.push(vault.ping(i).then(() => order.push(`v${i}`)));
  }
  for (let i = 0; i < 20; ++i) {
    calls.push(telemetry.ping(i).then(() => order.push(`t${i}`)));
  }
  await Promise.all(calls);
  const elapsedMs = Math.round((performance.now() - startedAt) * 100) / 100;
  const firstTelemetry = order.indexOf('t0');
  const lastVault = order.indexOf('v19');

  // Microtask storm inside "vault" must not starve "telemetry".
  const stormOrder = [];
  await Promise.all([
    vault.microtaskStorm(320).then((steps) => stormOrder.push(`vault-storm:${steps}`)),
    telemetry.ping(1).then(() => stormOrder.push('telemetry-ping')),
  ]);

  return Response.json({
    ok:
      order.length === 40 &&
      firstTelemetry >= 0 &&
      firstTelemetry < lastVault &&
      stormOrder[0] === 'telemetry-ping' &&
      stormOrder[1] === 'vault-storm:320',
    totalCalls: order.length,
    firstTelemetry,
    lastVault,
    elapsedMs,
    stormOrder,
  });
}

async function handleM4aErrors() {
  const checks = {};

  try {
    await vault.failType();
  } catch (err) {
    checks.typeError = err instanceof TypeError && err.message === 'bad argument type';
  }
  try {
    await vault.failRange();
  } catch (err) {
    checks.rangeError = err instanceof RangeError && err.message === 'index out of range';
  }
  try {
    await vault.failSyntax();
  } catch (err) {
    checks.syntaxError = err instanceof SyntaxError && err.message === 'invalid query syntax';
  }
  try {
    await vault.failUri();
  } catch (err) {
    checks.uriError = err instanceof URIError && err.message === 'malformed URI component';
  }
  try {
    await vault.failDom();
  } catch (err) {
    checks.domException =
      err instanceof DOMException &&
      err.name === 'NotFoundError' &&
      err.message === 'record missing';
  }
  try {
    await vault.failQuota();
  } catch (err) {
    checks.quotaExceededError =
      err instanceof QuotaExceededError &&
      err.quota === 1024 &&
      err.requested === 4096 &&
      err.message === 'storage budget exceeded';
  }
  try {
    await vault.failHostileGetter();
  } catch (err) {
    checks.hostileGetterFailClosed = err.message === 'binding call failed';
  }
  try {
    await vault.failProxy();
  } catch (err) {
    checks.proxyRejectionFailClosed = err.message === 'binding call failed';
  }
  checks.hostileGetterNeverRan = (await vault.hostileErrorGetterRan()) === false;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleM4aOptions() {
  await vault.resetOptionStats();
  const checks = {};

  // 1. Pre-aborted signal rejects immediately without entering the Binding.
  const preAbort = new AbortController();
  preAbort.abort(new DOMException('already-aborted', 'AbortError'));
  try {
    await vault.countedPing('a', { signal: preAbort.signal });
  } catch (err) {
    checks.preAborted =
      err instanceof DOMException && err.name === 'AbortError' && err.message === 'already-aborted';
  }

  // 2. timeoutMs: 0 rejects immediately without entering the Binding.
  try {
    await vault.countedPing('b', { timeoutMs: 0 });
  } catch (err) {
    checks.zeroTimeout = err instanceof DOMException && err.name === 'TimeoutError';
  }

  // 3. Invalid options fail closed with TypeError and never execute getters.
  let optionGetterRan = false;
  const getterOpt = Object.defineProperty({}, 'timeoutMs', {
    enumerable: true,
    get() {
      optionGetterRan = true;
      return 100;
    },
  });
  let badOptionCount = 0;
  for (const invalid of [
    { unknownKey: true },
    { timeoutMs: -5 },
    { signal: { aborted: false } },
    getterOpt,
  ]) {
    try {
      await vault.countedPing('c', invalid);
    } catch (err) {
      if (err instanceof TypeError) {
        badOptionCount++;
      }
    }
  }
  checks.invalidOptionsRejected = badOptionCount === 4 && !optionGetterRan;
  checks.bindingNotEnteredYet = (await vault.optionStats()).invocations === 0;

  // 4. In-flight AbortSignal propagates structured abort reason to call.signal.
  const liveAbort = new AbortController();
  const waitCall = vault.waitForAbort(null, { signal: liveAbort.signal });
  await new Promise((r) => setTimeout(r, 15));
  liveAbort.abort(new DOMException('caller-canceled', 'AbortError'));
  try {
    await waitCall;
  } catch (err) {
    checks.inflightAbort =
      err instanceof DOMException && err.name === 'AbortError' && err.message === 'caller-canceled';
  }
  await new Promise((r) => setTimeout(r, 30));
  const statsAfterAbort = await vault.optionStats();
  checks.bindingObservedAbort =
    statsAfterAbort.lastAbort.name === 'AbortError' &&
    statsAfterAbort.lastAbort.message === 'caller-canceled';

  // 5. Per-call timeoutMs cancels the call while the parent HTTP request survives.
  try {
    await vault.waitForAbort(null, { timeoutMs: 25 });
  } catch (err) {
    checks.perCallTimeout = err instanceof DOMException && err.name === 'TimeoutError';
  }
  await new Promise((r) => setTimeout(r, 30));
  checks.bindingObservedTimeout = (await vault.optionStats()).lastAbort.name === 'TimeoutError';

  // 6. Secret-backed Binding call: secret stays in Binding, never exposed to User.
  const signed = await vault.signPayload('hello-capsid-v0.4.0');
  checks.secretIsolatedSign =
    signed.namespace === 'showcase-v2' &&
    signed.secretConfigured === true &&
    typeof signed.digest === 'string' &&
    signed.digest.length === 64;

  // 7. Reusing a single AbortSignal across 40 sequential calls refills cleanly.
  const shared = new AbortController();
  let okCalls = 0;
  for (let i = 0; i < 40; ++i) {
    if ((await vault.countedPing(i, { signal: shared.signal })) === `ok:${i}`) {
      okCalls++;
    }
  }
  checks.reusedSignalClean = okCalls === 40;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleM4aQuotas() {
  const checks = {};

  // 1. Per-request queued RPC byte quota (4 MiB limit) + atomic refill.
  const payload900k = new Uint8Array(900 * 1024);
  const queued = [];
  for (let i = 0; i < 6; ++i) {
    queued.push(
      vault.echoBytesLen(payload900k).then(
        (len) => ({ ok: true, len }),
        (err) => ({ ok: false, message: err?.message ?? String(err) }),
      ),
    );
  }
  const results = await Promise.all(queued);
  const quotaHit = results.some((r) => !r.ok && r.message.includes('byte quota'));
  const refilledLen = await vault.echoBytesLen(payload900k);
  checks.rpcByteQuotaAndRefill = quotaHit && refilledLen === payload900k.byteLength;

  // 2. Per-request active RpcTarget handle quota (32 limit) + refill after dispose.
  const handles = [];
  for (let i = 0; i < 32; ++i) {
    handles.push(await vault.openSession(`quota-${i}`));
  }
  let handleQuotaHit = false;
  try {
    await vault.openSession('quota-overflow');
  } catch (err) {
    handleQuotaHit = Boolean(err?.message?.includes('handle quota'));
  }
  for (const handle of handles) {
    handle.dispose();
  }
  await new Promise((r) => setTimeout(r, 30));
  const afterRefill = await vault.openSession('quota-refill-ok');
  const appendRes = await afterRefill.append('refilled');
  afterRefill.dispose();
  checks.handleQuotaAndRefill = handleQuotaHit && appendRes.count === 1;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleM4bReadableStream() {
  const checks = {};

  // 1. Binding -> User credit backpressure + byte-exact 320 KiB transfer.
  const stream = await vault.fastProducer();
  await new Promise((r) => setTimeout(r, 25));
  const pausedProduced = await vault.producedCount();
  checks.backpressurePausedProducer = pausedProduced > 0 && pausedProduced < 12;

  const reader = stream.getReader();
  let total = 0;
  const parts = [];
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    parts.push(value);
    total += value.byteLength;
  }
  const merged = new Uint8Array(total);
  let offset = 0;
  for (const part of parts) {
    merged.set(part, offset);
    offset += part.byteLength;
  }
  let bytesIntact = total === 20 * 16384;
  for (let i = 0; i < 20 && bytesIntact; ++i) {
    const expected = (i + 1) & 0xff;
    if (merged[i * 16384] !== expected || merged[i * 16384 + 16383] !== expected) {
      bytesIntact = false;
    }
  }
  checks.bindingToUser320k = bytesIntact && (await vault.producedCount()) === 20;

  // 2. User -> Binding streaming upload (150 KiB chunk > 64 KiB exercises credit slicing).
  const bigChunk = new Uint8Array(150000);
  let expectedChecksum = 0;
  for (let i = 0; i < bigChunk.length; ++i) {
    bigChunk[i] = (i * 7 + 3) & 0xff;
    expectedChecksum = (expectedChecksum + bigChunk[i]) % 65521;
  }
  const tailChunk = new Uint8Array([10, 20, 30]);
  for (const b of tailChunk) {
    expectedChecksum = (expectedChecksum + b) % 65521;
  }
  const uploadStream = new ReadableStream({
    start(controller) {
      controller.enqueue(bigChunk);
      controller.enqueue(tailChunk);
      controller.close();
    },
  });
  const uploadRes = await vault.digestUploadStream(uploadStream);
  checks.userToBindingSlicedUpload =
    uploadRes.totalBytes === 150003 &&
    uploadRes.checksum === expectedChecksum &&
    uploadRes.sha256.length === 64;

  // 3. Cancel propagation with structured DOMException reason.
  const cStream = await vault.cancelableStream();
  const cReader = cStream.getReader();
  const firstChunk = await cReader.read();
  await cReader.cancel(new DOMException('stop-reading', 'AbortError'));
  await new Promise((r) => setTimeout(r, 20));
  checks.cancelReasonPropagated =
    !firstChunk.done &&
    firstChunk.value.byteLength === 4 &&
    (await vault.lastStreamCancelReason()) === 'AbortError:stop-reading';

  // 4. Producer error propagation preserves RangeError subclass.
  const eStream = await vault.erroringStream();
  const eReader = eStream.getReader();
  const eFirst = await eReader.read();
  let rangeErrPropagated = false;
  try {
    await eReader.read();
  } catch (err) {
    rangeErrPropagated = err instanceof RangeError && err.message === 'producer-fault';
  }
  checks.errorSubclassPropagated = !eFirst.done && eFirst.value[0] === 9 && rangeErrPropagated;

  // 5. Locked, disturbed, and partial-clone rollback fail closed.
  const locked = new ReadableStream();
  locked.getReader();
  let lockedRejected = false;
  try {
    await vault.digestUploadStream(locked);
  } catch (err) {
    lockedRejected = Boolean(err?.message?.includes('locked'));
  }

  const disturbed = new ReadableStream({
    start(c) {
      c.enqueue(new Uint8Array([1]));
      c.close();
    },
  });
  await disturbed.getReader().read();
  let disturbedRejected = false;
  try {
    await vault.digestUploadStream(disturbed);
  } catch (err) {
    disturbedRejected = Boolean(
      err?.message?.includes('locked') || err?.message?.includes('disturbed'),
    );
  }

  const rollbackStream = new ReadableStream({
    start(c) {
      c.enqueue(encoder.encode('still-readable'));
      c.close();
    },
  });
  let partialRejected = false;
  try {
    await vault.digestUploadStream([rollbackStream, () => {}]);
  } catch {
    partialRejected = true;
  }
  const rolledBackText = await new Response(rollbackStream).text();
  checks.guardsAndRollback =
    lockedRejected && disturbedRejected && partialRejected && rolledBackText === 'still-readable';

  // 6. Late-result stream disposal after per-call timeoutMs.
  try {
    await vault.delayedStream(null, { timeoutMs: 15 });
  } catch {
    // Expected TimeoutError
  }
  await new Promise((r) => setTimeout(r, 65));
  checks.lateResultStreamDisposed = (await vault.wasLateStreamCanceled()) === true;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    pausedProduced,
    uploadSha256: uploadRes.sha256,
    checks,
  });
}

async function handleM4bWritableStream() {
  const checks = {};

  // 1. Binding -> User slow sink backpressure (> 64 KiB) + 200 KiB sliced write.
  const slowWs = await vault.openSlowSink();
  const writer = slowWs.getWriter();
  const buf200k = new Uint8Array(200000);
  let expectedSum = 0;
  for (let i = 0; i < buf200k.length; ++i) {
    buf200k[i] = (i * 11 + 5) & 0xff;
    expectedSum = (expectedSum + buf200k[i]) % 65521;
  }
  await writer.write(buf200k);
  await writer.close();
  const sinkStats = await vault.sinkStats();
  checks.slowSinkSliced200k =
    sinkStats.sinkBytes === 200000 &&
    sinkStats.sinkChecksum === expectedSum &&
    sinkStats.sinkClosed === true;

  // 2. Staged 2 MiB per-stream buffered byte cap fails closed.
  const cappedWs = await vault.openSlowSink();
  const cappedWriter = cappedWs.getWriter();
  let stagedCapRejected = false;
  try {
    await cappedWriter.write(new Uint8Array(2 * 1024 * 1024 + 1024));
  } catch (err) {
    stagedCapRejected = Boolean(err?.message?.includes('buffered byte limit'));
  }
  checks.staged2MiBCapRejected = stagedCapRejected;

  // 3. User -> Binding WritableStream.
  const received = [];
  let userClosed = false;
  const userSink = new WritableStream({
    write(chunk) {
      received.push(new TextDecoder().decode(chunk));
    },
    close() {
      userClosed = true;
    },
  });
  const writeResult = await vault.writeToUserSink(userSink);
  checks.userToBindingSink =
    writeResult === 'written' && received.join('') === 'hello-binding-v2' && userClosed;

  // 4. Sink error propagation preserves TypeError.
  const errWs = await vault.erroringSink();
  const errWriter = errWs.getWriter();
  await errWriter.write(new Uint8Array([1]));
  let sinkErrorPropagated = false;
  try {
    await errWriter.write(new Uint8Array([2]));
    await errWriter.close();
  } catch (err) {
    sinkErrorPropagated = err instanceof TypeError && err.message === 'sink-capacity-reached';
  }
  checks.sinkErrorPropagated = sinkErrorPropagated;

  // 5. Writer abort propagates structured DOMException reason to Binding sink.
  const abWs = await vault.abortableSink();
  const abWriter = abWs.getWriter();
  await abWriter.abort(new DOMException('abort-upload', 'AbortError'));
  await new Promise((r) => setTimeout(r, 20));
  checks.abortReasonPropagated =
    (await vault.sinkStats()).sinkAbortReason === 'AbortError:abort-upload';

  // 6. Locked WritableStream rejected fail-closed.
  const lockedWs = new WritableStream();
  lockedWs.getWriter();
  let lockedRejected = false;
  try {
    await vault.writeToUserSink(lockedWs);
  } catch (err) {
    lockedRejected = Boolean(err?.message?.includes('locked'));
  }
  checks.lockedWritableRejected = lockedRejected;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleM4cHttpProxies() {
  const checks = {};

  // 1. User -> Binding inline Request (<= 64 KiB) + metadata + bodyUsed protection.
  const req1 = new CustomRequest('https://api.example/items?page=1', {
    method: 'POST',
    credentials: 'include',
    redirect: 'manual',
    mode: 'cors',
    headers: { 'x-custom': 'capsid-v2' },
    body: 'inline-request-payload',
  });
  const echo1 = await vault.echoRequest(req1);
  checks.inlineRequestAndSubclass =
    echo1.isRequest &&
    echo1.url === 'https://api.example/items?page=1' &&
    echo1.method === 'POST' &&
    echo1.credentials === 'include' &&
    echo1.redirect === 'manual' &&
    echo1.mode === 'cors' &&
    echo1.referrer === null &&
    echo1.customHeader === 'capsid-v2' &&
    echo1.text === 'inline-request-payload' &&
    req1.bodyUsed === true;

  let reuseRejected = false;
  try {
    await vault.echoRequest(req1);
  } catch (err) {
    reuseRejected = Boolean(err?.message?.includes('already used'));
  }
  checks.usedRequestRejected = reuseRejected;

  // 2. User -> Binding streaming Request (> 64 KiB).
  const bigPayload = new Uint8Array(90000);
  let expectedSum = 0;
  for (let i = 0; i < bigPayload.length; ++i) {
    bigPayload[i] = (i * 3 + 7) & 0xff;
    expectedSum = (expectedSum + bigPayload[i]) % 65521;
  }
  const streamReq = new Request('https://api.example/upload', {
    method: 'PUT',
    body: new ReadableStream({
      start(c) {
        c.enqueue(bigPayload);
        c.close();
      },
    }),
    duplex: 'half',
  });
  const streamSum = await vault.sumStreamRequest(streamReq);
  checks.streamingRequest90k = streamSum.length === 90000 && streamSum.checksum === expectedSum;

  // 3. Pre-aborted, queued-aborted, and live-aborted Request.signal propagation.
  const preAc = new AbortController();
  preAc.abort(new DOMException('req-pre', 'AbortError'));
  const preInfo = await vault.checkRequestSignal(
    new Request('https://api.example/pre', { signal: preAc.signal }),
  );
  checks.preAbortedRequestSignal = preInfo.aborted && preInfo.message === 'req-pre';

  const queuedAc = new AbortController();
  const queuedPromise = vault.checkRequestSignal(
    new Request('https://api.example/queued', { signal: queuedAc.signal }),
  );
  queuedAc.abort(new DOMException('req-queued', 'AbortError'));
  const queuedInfo = await queuedPromise;
  checks.queuedAbortedRequestSignal =
    queuedInfo.hasSignal && queuedInfo.aborted && queuedInfo.message === 'req-queued';

  const liveAc = new AbortController();
  const livePromise = vault.waitRequestSignalAbort(
    new Request('https://api.example/live', { signal: liveAc.signal }),
  );
  setTimeout(() => liveAc.abort(new DOMException('req-live', 'AbortError')), 15);
  checks.liveAbortedRequestSignal = (await livePromise) === 'AbortError:req-live';

  // 4. Binding -> User inline Response, multi-value Set-Cookie, immutable headers, 204, Response.error().
  const res1 = await vault.inlineResponse();
  let immutableThrew = false;
  try {
    res1.headers.set('x-mutate', 'nope');
  } catch (err) {
    immutableThrew = err instanceof TypeError;
  }
  checks.inlineResponseAndImmutableHeaders =
    res1 instanceof Response &&
    res1.status === 201 &&
    res1.statusText === 'Created' &&
    res1.headers.get('x-source') === 'vault-binding' &&
    res1.headers.getSetCookie().join(' | ') === 'sid=alpha; Path=/ | theme=dark; Path=/' &&
    immutableThrew &&
    (await res1.text()) === 'inline-from-vault';

  const subRes = await vault.subResponse();
  checks.responseSubclass =
    subRes instanceof Response &&
    subRes.status === 202 &&
    subRes.headers.get('x-sub') === 'yes' &&
    (await subRes.text()) === 'subclass-response';

  const res204 = await vault.noContentResponse();
  checks.noContent204 = res204.status === 204 && res204.body === null;

  const resErrB2U = await vault.errorResponse();
  let errB2UImmutable = false;
  try {
    resErrB2U.headers.set('x-mut', '1');
  } catch (err) {
    errB2UImmutable = err instanceof TypeError;
  }
  const errU2B = await vault.checkErrorResponseImmutable(Response.error());
  checks.errorResponseImmutableBothWays =
    resErrB2U.type === 'error' &&
    resErrB2U.status === 0 &&
    errB2UImmutable &&
    errU2B.type === 'error' &&
    errU2B.status === 0 &&
    errU2B.setImmutable &&
    errU2B.appendImmutable &&
    errU2B.deleteImmutable;

  // 5. User -> Binding Response + atomic rollback on composite input failure.
  const u2bRes = await vault.echoResponse(new Response('user-response', { status: 202 }));
  const rollbackRes = new Response('keep-me-intact', { status: 200 });
  let compositeRejected = false;
  try {
    await vault.echoResponse([rollbackRes, () => {}]);
  } catch {
    compositeRejected = true;
  }
  checks.userToBindingAndAtomicRollback =
    u2bRes.status === 202 &&
    u2bRes.text === 'user-response' &&
    compositeRejected &&
    rollbackRes.bodyUsed === false &&
    (await rollbackRes.text()) === 'keep-me-intact';

  // 6. Negative controls: locked Request, already-used Response, spoofed prototype, WebSocket 101 Response.
  const lockedStream = new ReadableStream();
  lockedStream.getReader();
  let lockedReqRejected = false;
  try {
    await vault.echoRequest(
      new Request('https://api.example/locked', {
        method: 'POST',
        body: lockedStream,
        duplex: 'half',
      }),
    );
  } catch (err) {
    lockedReqRejected = Boolean(err?.message?.includes('locked'));
  }

  let usedResRejected = false;
  try {
    await vault.usedResponse();
  } catch (err) {
    usedResRejected = Boolean(err?.message?.includes('already used'));
  }

  const spoofReq = Object.create(Request.prototype);
  spoofReq.method = 'GET';
  spoofReq.url = 'https://spoof.example/';
  let spoofRejected = false;
  try {
    await vault.echoRequest(spoofReq);
  } catch {
    spoofRejected = true;
  }

  let wsResRejected = false;
  try {
    const pair = new WebSocketPair();
    const wsRes = new Response(null, { status: 101, webSocket: pair[0] });
    await vault.echoResponse(wsRes);
  } catch (err) {
    wsResRejected = Boolean(err?.message?.includes('WebSocket'));
  }
  checks.negativeControls = lockedReqRejected && usedResRejected && spoofRejected && wsResRejected;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleM4dRpcTarget() {
  const checks = {};
  checks.userHasNoRpcTargetGlobal = typeof globalThis.RpcTarget === 'undefined';

  // 1. Stateful RpcTarget handle + frozen null-prototype stub + nested cursor + stream + Response + errors.
  const session = await vault.openSession('session-primary');
  checks.frozenNullProtoStub = Object.getPrototypeOf(session) === null && Object.isFrozen(session);

  await session.append('alpha');
  await session.append('beta');
  await session.append('gamma');
  const summary = await session.summary();

  let typedErrorOk = false;
  try {
    await session.failType();
  } catch (err) {
    typedErrorOk =
      err instanceof TypeError && err.message === 'invalid operation on session session-primary';
  }

  const exportedStreamText = await new Response(await session.exportStream()).text();
  const exportedRes = await session.exportResponse();
  const exportedJson = await exportedRes.json();

  const { sessionLabel, cursor } = await session.openCursor();
  const page1 = await cursor.next(2);
  const page2 = await cursor.next(2);
  cursor[Symbol.dispose]();

  session.dispose();
  session.dispose(); // idempotent
  const postDisposeFenced = await expectAsyncInvalidState(() => session.append('after-dispose'));

  checks.statefulNestedAndDispose =
    summary.count === 3 &&
    summary.signature.length === 64 &&
    typedErrorOk &&
    exportedStreamText === '0:alpha\n1:beta\n2:gamma\n' &&
    exportedRes.status === 202 &&
    exportedJson.entries.length === 3 &&
    sessionLabel === 'session-primary' &&
    page1.items.join(',') === 'alpha,beta' &&
    page1.done === false &&
    page2.items.join(',') === 'gamma' &&
    page2.done === true &&
    postDisposeFenced;

  // 2. Method snapshot immunity (post-export property mutation cannot hijack handle calls).
  const mutHandle = await vault.openMutableSession('mutable-target');
  await vault.mutateLastSessionMethod();
  const mutAppend = await mutHandle.append('safe');
  mutHandle.dispose();
  checks.methodSnapshotImmutable = mutAppend.count === 1;

  // 3. Lease expiry triggers automatic disposal.
  const shortLease = await vault.openShortLeaseSession({ label: 'short-lease', leaseMs: 45 });
  const beforeExpiry = await shortLease.append('alive');
  await new Promise((r) => setTimeout(r, 85));
  checks.leaseAutoDisposed =
    beforeExpiry.count === 1 && (await expectAsyncInvalidState(() => shortLease.append('expired')));

  // 4. Fault-isolated throwing [Symbol.dispose]() via [Symbol.asyncDispose]().
  const throwingHandle = await vault.openThrowingDispose();
  const aliveMsg = await throwingHandle.ping();
  await throwingHandle[Symbol.asyncDispose]();
  await new Promise((r) => setTimeout(r, 25));
  checks.throwingDisposeFaultIsolated = aliveMsg === 'alive-before-dispose';

  // 5. GC finalizer fallback (drop without calling dispose).
  {
    let ephemeral = await vault.openSession('gc-ephemeral');
    await ephemeral.append('temp');
    ephemeral = null;
  }
  await new Promise((r) => setTimeout(r, 35));

  // 6. Late-result recursive, cycle-safe, deduped disposal (nested RpcTarget + streams).
  let lateTimedOut = false;
  try {
    await vault.openLateNestedResources(null, { timeoutMs: 12 });
  } catch (err) {
    lateTimedOut = err instanceof DOMException && err.name === 'TimeoutError';
  }
  await new Promise((r) => setTimeout(r, 85));

  // 7. Negative controls: non-RpcTarget, getter on RpcTarget, Proxy around RpcTarget, passing handle back.
  let ordinaryRejected = false;
  try {
    await vault.openOrdinary();
  } catch {
    ordinaryRejected = true;
  }
  let getterRejected = false;
  try {
    await vault.openGetterTarget();
  } catch (err) {
    getterRejected = Boolean(err?.message?.includes('accessor'));
  }
  let proxyRejected = false;
  try {
    await vault.openProxyTarget();
  } catch {
    proxyRejected = true;
  }
  const passBack = await vault.openSession('pass-back-check');
  let passBackRejected = false;
  try {
    await vault.acceptAny(passBack);
  } catch {
    passBackRejected = true;
  }
  passBack.dispose();
  await new Promise((r) => setTimeout(r, 20));

  const stats = await vault.targetStats();
  checks.gcAndDisposedAudit =
    stats.disposed.includes('session-primary') &&
    stats.disposed.includes('cursor:session-primary') &&
    stats.disposed.includes('short-lease') &&
    stats.disposed.includes('throwing-dispose') &&
    stats.disposed.includes('gc-ephemeral') &&
    !stats.mutatedGetterRan;
  checks.lateNestedDisposedOnce =
    lateTimedOut &&
    stats.lateDisposeCounts['late-one'] === 1 &&
    stats.lateDisposeCounts['late-shared'] === 1 &&
    stats.lateNestedStreams.direct === true &&
    stats.lateNestedStreams.reqBody === true &&
    stats.lateNestedStreams.resBody === true;
  checks.maliciousProviderGuards =
    ordinaryRejected &&
    getterRejected &&
    proxyRejected &&
    passBackRejected &&
    !stats.targetGetterTripped &&
    !stats.targetProxyTripped;

  return Response.json({
    ok: Object.values(checks).every(Boolean),
    checks,
  });
}

async function handleFencingHold() {
  const readable = await vault.openFencedReadable();
  const writable = await vault.openFencedWritable();
  const handle = await vault.openSession('fenced-owner');
  await handle.append('owner-secret');

  globalThis.__capsidFenceDemo = { readable, writable, handle };
  await new Promise((resolve) => {
    releaseFenceGate = resolve;
  });
  globalThis.__capsidFenceDemo = null;
  releaseFenceGate = null;

  // Owner request MUST still read/write/invoke cleanly after foreign attempts were rejected!
  const reader = readable.getReader();
  const readBytes = [];
  for (;;) {
    const { done, value } = await reader.read();
    if (done) {
      break;
    }
    for (const b of value) {
      readBytes.push(b);
    }
  }
  const writer = writable.getWriter();
  await writer.write(new Uint8Array([9, 8, 7, 6]));
  await writer.close();
  const sink = await vault.fencedSinkStats();
  const summary = await handle.summary();
  handle.dispose();

  const ok =
    readBytes.join(',') === '17,34,51,68,85,102' &&
    sink.bytes.join(',') === '9,8,7,6' &&
    sink.closed === true &&
    summary.count === 1;
  return Response.json({ ok, role: 'owner', readBytes, sink, count: summary.count });
}

async function handleFencingSteal() {
  const held = globalThis.__capsidFenceDemo;
  if (!held) {
    return Response.json(
      { ok: false, error: 'call /m4d/fencing-hold concurrently first' },
      { status: 409 },
    );
  }
  const { readable, writable, handle } = held;
  const checks = {
    readableGetReader:
      expectSyncInvalidState(() => readable.getReader()) && readable.locked === false,
    readableCancel:
      (await expectAsyncInvalidState(() => readable.cancel('stolen'))) && readable.locked === false,
    readableTee: expectSyncInvalidState(() => readable.tee()) && readable.locked === false,
    readableRetransfer:
      (await expectAsyncInvalidState(() => vault.digestUploadStream(readable))) &&
      readable.locked === false,
    writableGetWriter:
      expectSyncInvalidState(() => writable.getWriter()) && writable.locked === false,
    writableAbort:
      (await expectAsyncInvalidState(() => writable.abort('stolen'))) && writable.locked === false,
    writableClose:
      (await expectAsyncInvalidState(() => writable.close())) && writable.locked === false,
    handleCall: await expectAsyncInvalidState(() => handle.summary()),
    handleDispose: expectSyncInvalidState(() => handle.dispose()),
    handleRetransfer: await expectAsyncInvalidState(() => vault.acceptAny(handle)),
  };

  const wake = releaseFenceGate;
  releaseFenceGate = null;
  if (wake) {
    wake();
  }
  return Response.json({
    ok: Object.values(checks).every(Boolean),
    role: 'foreign',
    checks,
  });
}

async function handleRequestEndLeak() {
  const leaked = await vault.openSession('request-end-auto-cleanup');
  await leaked.append('held-until-response-ends');
  // Deliberately retain on globalThis without calling .dispose().
  globalThis.__capsidHeldPastReqEnd = leaked;
  return Response.json({ ok: true, leakedLabel: 'request-end-auto-cleanup' });
}

async function handleRequestEndVerify() {
  const leaked = globalThis.__capsidHeldPastReqEnd;
  globalThis.__capsidHeldPastReqEnd = null;
  if (!leaked) {
    return Response.json({ ok: false, error: 'call /m4d/request-end-leak first' }, { status: 409 });
  }
  const deadAfterReqEnd = await expectAsyncInvalidState(() => leaked.summary());
  const stats = await vault.targetStats();
  const disposedAtEnd = stats.disposed.includes('request-end-auto-cleanup');
  return Response.json({
    ok: deadAfterReqEnd && disposedAtEnd,
    deadAfterReqEnd,
    disposedAtEnd,
  });
}

export default {
  async fetch(request) {
    const { pathname } = new URL(request.url);

    switch (pathname) {
      case '/':
        return Response.json({
          name: 'Capsid Binding RPC v2 Showcase (v0.4.0)',
          routes: [
            '/m4a/fair-scheduling',
            '/m4a/errors',
            '/m4a/options',
            '/m4a/quotas',
            '/m4b/readable-stream',
            '/m4b/writable-stream',
            '/m4b/stream-passthrough',
            '/m4c/http-proxies',
            '/m4c/proxy-response',
            '/m4d/rpc-target',
            '/m4d/fencing-hold',
            '/m4d/fencing-steal',
            '/m4d/request-end-leak',
            '/m4d/request-end-verify',
          ],
        });
      case '/m4a/fair-scheduling':
        return handleM4aFairScheduling();
      case '/m4a/errors':
        return handleM4aErrors();
      case '/m4a/options':
        return handleM4aOptions();
      case '/m4a/quotas':
        return handleM4aQuotas();
      case '/m4b/readable-stream':
        return handleM4bReadableStream();
      case '/m4b/writable-stream':
        return handleM4bWritableStream();
      case '/m4b/stream-passthrough':
        return new Response(await vault.openEventStream(6), {
          headers: { 'content-type': 'application/x-ndjson' },
        });
      case '/m4c/http-proxies':
        return handleM4cHttpProxies();
      case '/m4c/proxy-response':
        return await vault.streamReport();
      case '/m4d/rpc-target':
        return handleM4dRpcTarget();
      case '/m4d/fencing-hold':
        return handleFencingHold();
      case '/m4d/fencing-steal':
        return handleFencingSteal();
      case '/m4d/request-end-leak':
        return handleRequestEndLeak();
      case '/m4d/request-end-verify':
        return handleRequestEndVerify();
      default:
        return Response.json({ error: 'not found', pathname }, { status: 404 });
    }
  },
};
