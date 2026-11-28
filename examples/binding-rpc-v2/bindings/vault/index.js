import { createHash } from 'capsid:hashing';
import uuid from 'capsid:uuid';

// Trusted Binding Runtime #1 ("vault")
// Demonstrates the full Binding RPC v2 (v0.4.0) surface inside an isolated
// QuickJS Binding Runtime:
//   - M4A: fair scheduling, microtask-storm preemption, structured errors,
//          { signal, timeoutMs } call options, secret isolation + log redaction
//   - M4B: bidirectional credit-based ReadableStream & WritableStream proxies
//   - M4C: controlled Request & Response proxies (inline <=64 KiB & streaming)
//   - M4D: native-branded RpcTarget handles, nested handles, leases, disposal

export default function createVaultBinding({ config, secrets, log }) {
  const namespace = config.namespace || 'demo';
  const signingKey = secrets.signingKey || '';
  const encoder = new TextEncoder();

  let seq = 0;
  let invocations = 0;
  let lastAbort = { name: '', message: '', plain: null };
  let hostileErrorGetterRan = false;

  // M4B stream counters
  let producedChunks = 0;
  let lastStreamCancelReason = null;
  let lateStreamCanceled = false;
  let sinkBytes = 0;
  let sinkChecksum = 0;
  let sinkClosed = false;
  let sinkAbortReason = null;
  let fencedSinkChunks = [];
  let fencedSinkClosed = false;

  // M4D RpcTarget counters & negative controls
  const disposedLog = [];
  const lateDisposeCounts = Object.create(null);
  const lateNestedStreams = { direct: false, reqBody: false, resBody: false };
  let targetGetterTripped = false;
  let targetProxyTripped = false;
  let lastMutableSession = null;
  let mutatedGetterRan = false;

  function keyedDigestHex(payloadBytes) {
    const hash = createHash('sha256');
    hash.update(encoder.encode(`${namespace}:${signingKey}:`));
    hash.update(payloadBytes);
    return hash.digest();
  }

  class QueryCursor extends RpcTarget {
    constructor(sessionId, items) {
      super();
      this.sessionId = sessionId;
      this.items = items.slice();
      this.offset = 0;
    }

    next(limit = 2) {
      const slice = this.items.slice(this.offset, this.offset + limit);
      this.offset += slice.length;
      return {
        sessionId: this.sessionId,
        items: slice,
        done: this.offset >= this.items.length,
      };
    }

    [Symbol.dispose]() {
      disposedLog.push(`cursor:${this.sessionId}`);
    }
  }

  class DocumentSession extends RpcTarget {
    constructor(label, leaseMs) {
      super(leaseMs !== undefined ? { leaseMs } : undefined);
      this.id = uuid.v4();
      this.label = label;
      this.entries = [];
    }

    append(text) {
      this.entries.push(String(text));
      return { count: this.entries.length, label: this.label };
    }

    summary() {
      const joined = this.entries.join('|');
      return {
        id: this.id,
        label: this.label,
        count: this.entries.length,
        signature: keyedDigestHex(encoder.encode(joined)),
      };
    }

    failType() {
      throw new TypeError(`invalid operation on session ${this.label}`);
    }

    openCursor() {
      return {
        sessionLabel: this.label,
        cursor: new QueryCursor(this.label, this.entries),
      };
    }

    exportStream() {
      const lines = this.entries.map((entry, idx) => `${idx}:${entry}\n`);
      let i = 0;
      return new ReadableStream({
        pull(controller) {
          if (i >= lines.length) {
            controller.close();
            return;
          }
          controller.enqueue(encoder.encode(lines[i++]));
        },
      });
    }

    exportResponse() {
      const payload = JSON.stringify({
        label: this.label,
        entries: this.entries,
      });
      return new Response(payload, {
        status: 202,
        statusText: 'Accepted',
        headers: {
          'content-type': 'application/json',
          'x-session-label': this.label,
        },
      });
    }

    [Symbol.dispose]() {
      disposedLog.push(this.label);
    }
  }

  class ThrowingDisposeTarget extends RpcTarget {
    ping() {
      return 'alive-before-dispose';
    }

    [Symbol.dispose]() {
      disposedLog.push('throwing-dispose');
      throw new Error('simulated cleanup error (must stay fault-isolated)');
    }
  }

  class GetterTarget extends RpcTarget {
    ok() {
      return 1;
    }

    get hostile() {
      targetGetterTripped = true;
      throw new Error('hostile getter must never run during export');
    }
  }

  class CountedLateTarget extends RpcTarget {
    constructor(tag) {
      super();
      this.tag = tag;
    }

    ping() {
      return this.tag;
    }

    [Symbol.dispose]() {
      lateDisposeCounts[this.tag] = (lateDisposeCounts[this.tag] || 0) + 1;
    }
  }

  class OrdinaryNonTarget {
    ping() {
      return 'should-be-rejected';
    }
  }

  class SubResponse extends Response {}

  return {
    // -------------------- M4A: Scheduling & Secrets --------------------
    ping(n, call) {
      return {
        binding: 'vault',
        n,
        seq: ++seq,
        requestId: call.requestId,
        namespace,
      };
    },

    microtaskStorm(steps) {
      return new Promise((resolve) => {
        let count = 0;
        function tick() {
          if (++count >= steps) {
            resolve(count);
          } else {
            Promise.resolve().then(tick);
          }
        }
        Promise.resolve().then(tick);
      });
    },

    signPayload(text, call) {
      const digest = keyedDigestHex(encoder.encode(String(text)));
      // Secret plaintext is automatically redacted from structured Binding logs.
      log.info(`signed payload using ${signingKey}`, {
        namespace,
        digestPrefix: digest.slice(0, 12),
        requestId: String(call.requestId),
      });
      return {
        namespace,
        digest,
        secretConfigured: signingKey.length > 0,
      };
    },

    echoBytesLen(buf) {
      return buf ? buf.byteLength : 0;
    },

    // -------------------- M4A: Structured Remote Errors --------------------
    failType() {
      throw new TypeError('bad argument type');
    },
    failRange() {
      return Promise.reject(new RangeError('index out of range'));
    },
    failSyntax() {
      throw new SyntaxError('invalid query syntax');
    },
    failUri() {
      throw new URIError('malformed URI component');
    },
    failDom() {
      throw new DOMException('record missing', 'NotFoundError');
    },
    failQuota() {
      throw new QuotaExceededError('storage budget exceeded', {
        quota: 1024,
        requested: 4096,
      });
    },
    failHostileGetter() {
      const err = new Error('original');
      Object.defineProperty(err, 'message', {
        configurable: true,
        get() {
          hostileErrorGetterRan = true;
          return 'tampered';
        },
      });
      throw err;
    },
    failProxy() {
      return Promise.reject(
        new Proxy(
          {},
          {
            get() {
              hostileErrorGetterRan = true;
              return 'tampered';
            },
          },
        ),
      );
    },
    hostileErrorGetterRan() {
      return hostileErrorGetterRan;
    },

    // -------------------- M4A: Call Options & Cancellation --------------------
    resetOptionStats() {
      invocations = 0;
      lastAbort = { name: '', message: '', plain: null };
      return true;
    },
    optionStats() {
      return { invocations, lastAbort };
    },
    countedPing(tag) {
      invocations++;
      return `ok:${tag}`;
    },
    waitForAbort(_input, call) {
      invocations++;
      return new Promise((resolve) => {
        const record = () => {
          const reason = call.signal.reason;
          if (reason && typeof reason === 'object' && 'name' in reason) {
            lastAbort = {
              name: String(reason.name),
              message: String(reason.message),
              plain: null,
            };
          } else {
            lastAbort = { name: '', message: '', plain: reason };
          }
          // Resolve late after cancellation to verify late-result handling.
          setTimeout(() => resolve('late-value'), 15);
        };
        if (call.signal.aborted) {
          record();
        } else {
          call.signal.addEventListener('abort', record);
        }
      });
    },

    // -------------------- M4B: Bidirectional Stream Proxies --------------------
    fastProducer() {
      producedChunks = 0;
      return new ReadableStream({
        pull(controller) {
          if (producedChunks >= 20) {
            controller.close();
            return;
          }
          const chunk = new Uint8Array(16384);
          chunk.fill((producedChunks + 1) & 0xff);
          producedChunks++;
          controller.enqueue(chunk);
        },
      });
    },
    producedCount() {
      return producedChunks;
    },
    async digestUploadStream(stream) {
      const reader = stream.getReader();
      const hash = createHash('sha256');
      let totalBytes = 0;
      let chunkCount = 0;
      let checksum = 0;
      for (;;) {
        const { done, value } = await reader.read();
        if (done) {
          break;
        }
        chunkCount++;
        totalBytes += value.byteLength;
        hash.update(value);
        for (let i = 0; i < value.byteLength; ++i) {
          checksum = (checksum + value[i]) % 65521;
        }
      }
      return { totalBytes, chunkCount, checksum, sha256: hash.digest() };
    },
    cancelableStream() {
      lastStreamCancelReason = null;
      return new ReadableStream({
        pull(controller) {
          controller.enqueue(new Uint8Array([1, 2, 3, 4]));
        },
        cancel(reason) {
          lastStreamCancelReason = reason ? `${reason.name}:${reason.message}` : 'none';
        },
      });
    },
    lastStreamCancelReason() {
      return lastStreamCancelReason;
    },
    erroringStream() {
      let n = 0;
      return new ReadableStream({
        pull(controller) {
          if (n++ === 0) {
            controller.enqueue(new Uint8Array([9, 8]));
          } else {
            controller.error(new RangeError('producer-fault'));
          }
        },
      });
    },
    delayedStream() {
      lateStreamCanceled = false;
      return new Promise((resolve) => {
        setTimeout(() => {
          resolve(
            new ReadableStream({
              pull(controller) {
                controller.enqueue(new Uint8Array([1]));
              },
              cancel() {
                lateStreamCanceled = true;
              },
            }),
          );
        }, 45);
      });
    },
    wasLateStreamCanceled() {
      return lateStreamCanceled;
    },
    openEventStream(count = 6) {
      let i = 0;
      return new ReadableStream({
        pull(controller) {
          if (i >= count) {
            controller.close();
            return;
          }
          const line =
            JSON.stringify({
              seq: i + 1,
              namespace,
              event: `chunk-${i + 1}`,
            }) + '\n';
          i++;
          controller.enqueue(encoder.encode(line));
        },
      });
    },
    openSlowSink() {
      sinkBytes = 0;
      sinkChecksum = 0;
      sinkClosed = false;
      return new WritableStream({
        async write(chunk) {
          await new Promise((r) => setTimeout(r, 2));
          sinkBytes += chunk.byteLength;
          for (let i = 0; i < chunk.byteLength; ++i) {
            sinkChecksum = (sinkChecksum + chunk[i]) % 65521;
          }
        },
        close() {
          sinkClosed = true;
        },
      });
    },
    sinkStats() {
      return { sinkBytes, sinkChecksum, sinkClosed, sinkAbortReason };
    },
    erroringSink() {
      let writes = 0;
      return new WritableStream({
        write() {
          if (++writes === 2) {
            throw new TypeError('sink-capacity-reached');
          }
        },
      });
    },
    abortableSink() {
      sinkAbortReason = null;
      return new WritableStream({
        write() {},
        abort(reason) {
          sinkAbortReason = reason ? `${reason.name}:${reason.message}` : 'none';
        },
      });
    },
    async writeToUserSink(writable) {
      const writer = writable.getWriter();
      await writer.write(encoder.encode('hello-'));
      await writer.write(encoder.encode('binding-v2'));
      await writer.close();
      return 'written';
    },

    // -------------------- M4C: Request & Response Proxies --------------------
    async echoRequest(req) {
      const text = await req.text();
      return {
        isRequest: req instanceof Request,
        url: req.url,
        method: req.method,
        credentials: req.credentials,
        redirect: req.redirect,
        mode: req.mode,
        referrer: req.referrer,
        customHeader: req.headers.get('x-custom'),
        text,
      };
    },
    async sumStreamRequest(req) {
      const bytes = new Uint8Array(await req.arrayBuffer());
      let checksum = 0;
      for (let i = 0; i < bytes.length; ++i) {
        checksum = (checksum + bytes[i]) % 65521;
      }
      return { length: bytes.length, checksum };
    },
    checkRequestSignal(req) {
      return {
        hasSignal: Boolean(req.signal),
        aborted: req.signal ? req.signal.aborted : false,
        name: req.signal?.reason?.name ?? '',
        message: req.signal?.reason?.message ?? '',
      };
    },
    waitRequestSignalAbort(req) {
      return new Promise((resolve) => {
        if (!req.signal) {
          resolve('no-signal');
          return;
        }
        if (req.signal.aborted) {
          resolve(`${req.signal.reason?.name}:${req.signal.reason?.message}`);
          return;
        }
        req.signal.addEventListener('abort', () => {
          resolve(`${req.signal.reason?.name}:${req.signal.reason?.message}`);
        });
      });
    },
    inlineResponse() {
      const headers = new Headers();
      headers.append('x-source', 'vault-binding');
      headers.append('set-cookie', 'sid=alpha; Path=/');
      headers.append('set-cookie', 'theme=dark; Path=/');
      return new Response('inline-from-vault', {
        status: 201,
        statusText: 'Created',
        headers,
      });
    },
    subResponse() {
      return new SubResponse('subclass-response', {
        status: 202,
        headers: { 'x-sub': 'yes' },
      });
    },
    noContentResponse() {
      return new Response(null, { status: 204 });
    },
    errorResponse() {
      return Response.error();
    },
    checkErrorResponseImmutable(res) {
      const throwsTypeError = (fn) => {
        try {
          fn();
          return false;
        } catch (err) {
          return err instanceof TypeError;
        }
      };
      return {
        type: res.type,
        status: res.status,
        setImmutable: throwsTypeError(() => res.headers.set('x-mut', '1')),
        appendImmutable: throwsTypeError(() => res.headers.append('x-mut', '1')),
        deleteImmutable: throwsTypeError(() => res.headers.delete('x-mut')),
      };
    },
    streamReport() {
      let i = 0;
      const body = new ReadableStream({
        pull(controller) {
          if (i >= 5) {
            controller.close();
            return;
          }
          const chunk = new Uint8Array(16384);
          chunk.fill(65 + i); // 'A'..'E' -> 80 KiB total (> 64 KiB streaming path)
          i++;
          controller.enqueue(chunk);
        },
      });
      return new Response(body, {
        status: 200,
        headers: {
          'content-type': 'application/octet-stream',
          'x-binding-stream': 'vault-report',
        },
      });
    },
    async usedResponse() {
      const res = new Response('already-read');
      await res.text();
      return res;
    },
    async echoResponse(res) {
      return { status: res.status, text: await res.text() };
    },

    // -------------------- M4D: RpcTarget Handles & Fencing --------------------
    openSession(label) {
      return new DocumentSession(label);
    },
    openShortLeaseSession({ label, leaseMs = 50 }) {
      return new DocumentSession(label, leaseMs);
    },
    openMutableSession(label) {
      lastMutableSession = new DocumentSession(label);
      return lastMutableSession;
    },
    mutateLastSessionMethod() {
      if (!lastMutableSession) {
        return false;
      }
      Object.defineProperty(lastMutableSession, 'append', {
        configurable: true,
        get() {
          mutatedGetterRan = true;
          throw new Error('mutated getter must not run');
        },
      });
      return true;
    },
    openThrowingDispose() {
      return new ThrowingDisposeTarget();
    },
    openOrdinary() {
      return new OrdinaryNonTarget();
    },
    openGetterTarget() {
      return new GetterTarget();
    },
    openProxyTarget() {
      const target = new DocumentSession('proxied');
      return new Proxy(target, {
        ownKeys() {
          targetProxyTripped = true;
          return ['append'];
        },
      });
    },
    openLateNestedResources() {
      lateNestedStreams.direct = false;
      lateNestedStreams.reqBody = false;
      lateNestedStreams.resBody = false;
      return new Promise((resolve) => {
        setTimeout(() => {
          const single = new CountedLateTarget('late-one');
          const shared = new CountedLateTarget('late-shared');
          const s1 = new ReadableStream({
            cancel() {
              lateNestedStreams.direct = true;
            },
          });
          const s2 = new ReadableStream({
            cancel() {
              lateNestedStreams.reqBody = true;
            },
          });
          const s3 = new ReadableStream({
            cancel() {
              lateNestedStreams.resBody = true;
            },
          });
          const graph = {
            targets: [single, shared],
            duplicateRef: shared,
            stream: s1,
            req: new Request('https://late.example/upload', {
              method: 'POST',
              body: s2,
              duplex: 'half',
            }),
            res: new Response(s3, { status: 200 }),
          };
          graph.self = graph; // cycle guard check
          resolve(graph);
        }, 40);
      });
    },
    openFencedReadable() {
      const chunks = [
        new Uint8Array([0x11, 0x22]),
        new Uint8Array([0x33, 0x44]),
        new Uint8Array([0x55, 0x66]),
      ];
      let idx = 0;
      return new ReadableStream({
        pull(controller) {
          if (idx >= chunks.length) {
            controller.close();
            return;
          }
          controller.enqueue(chunks[idx++]);
        },
      });
    },
    openFencedWritable() {
      fencedSinkChunks = [];
      fencedSinkClosed = false;
      return new WritableStream({
        write(chunk) {
          fencedSinkChunks.push(Array.from(chunk));
        },
        close() {
          fencedSinkClosed = true;
        },
      });
    },
    fencedSinkStats() {
      return { bytes: fencedSinkChunks.flat(), closed: fencedSinkClosed };
    },
    acceptAny(value) {
      return value;
    },
    targetStats() {
      return {
        disposed: disposedLog.slice(),
        lateDisposeCounts: { ...lateDisposeCounts },
        lateNestedStreams: { ...lateNestedStreams },
        targetGetterTripped,
        targetProxyTripped,
        mutatedGetterRan,
      };
    },
  };
}
