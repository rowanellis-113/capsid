# workerd WebSocket Semantics and Compatibility Scope

Reviewed 2026-11-27 against the `dev/v0.3.2` source and locked reference (published prerelease `v0.3.1-beta.1`).
This is a bounded compatibility reference, not a browser/workerd certification
or a pending-release diary. [Current status](release-status.md) identifies the
actual released source, raw standards conflicts and independent performance scope.

## Reference and Execution Boundary

- workerd release `v1.20261010.1`, source
  `8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f`;
- compatibility date `2026-08-20`, no explicit compatibility flags;
- platform-specific binaries, compressed assets and client/tool identities in
  the [reference lock](../../tests/fixtures/workerd-ws/reference-lock.json) and
  [native Windows lock](../../tests/fixtures/workerd-ws/reference-lock-windows.json);
- source-bound required gate in
  [`tools/run-workerd-ws-gate.mjs`](../../tools/run-workerd-ws-gate.mjs).

The Linux comparison uses the reference plus Capsid's unified local dev Host
(single-worker and multi-worker harness legs, historically labeled
`single-worker` / `static-pool`) and Linux managed mode. Native Windows/macOS qualification uses its actual supported
profile, not an assumption that managed mode exists there. Node ws and Python
websockets are independent clients/servers, not aliases for the same Node
implementation. TLS fixtures use the declared CA; verification is not disabled.

Required reports pin binaries, fixture/runner/client sources, exact case
inventories and natural exits. Empty/duplicate/filtered selection, missing
backends, zero assertions, malformed reports and unlisted differences fail the
gate. Portable metadata/control tests are not live reference evidence. Old
checkpoint test counts are retained in the historical snapshot, not displayed
as today's registration or acceptance count.

## Pair, Transfer and Acceptance

A pair exposes indexed endpoints and an iterator. The inbound idiom is:

```js
export default {
  fetch(request) {
    const [client, server] = Object.values(new WebSocketPair());
    server.accept();
    return new Response(null, { status: 101, webSocket: client });
  },
};
```

This is a minimal bundle, not a complete Host launch; supply an actual Upgrade
request using the [quick-start tools/configuration](../guides/run-application.md). Acceptance
and response coupling apply to different endpoints. Repeated ordinary accept
returns; accepting a coupled endpoint is an error. The option is
`allowHalfOpen`, not a hibernation-only extension.

A pair/Response transferred by an earlier HTTP request cannot be reused by a
later request. Fresh per-request pairs remain successful controls. A previously
accepted endpoint can reach a valid 101 and then abort the transport on both
engines; that is a commit failure, not successful endpoint coupling.

Response construction separates endpoint branding from transfer. Non-101 with
an endpoint throws RangeError. Fixed zero-length bodies become null; even an
already-empty ReadableStream is rejected for 101. Actual bytes determine body
emptiness, not a shadowed length/conversion property. Empty body adds no implicit
Content-Type; an explicit header is preserved. Upgraded Response cloning is
rejected.

101 can be visible before delayed local accept. Early DATA/ordinary Close stays
buffered until accept, and clean Close preserves preceding DATA. Transport
failure interrupts without delivering into an unaccepted endpoint. Capsid's
provisional coupling is bounded by the earlier of the request deadline and ten
seconds. Never-accept expiry and ACK/quota cleanup are explicit resource policy,
not an inferred reference parity claim.

## Events, Binary Values and Conversion

The event map is open/message/close/error, not messageerror. The pinned worker
profile defaults binaryType to `blob` under this date; a Node client's default
is a separate property. Invalid assignment retains the current value. Echo
fixtures select arraybuffer explicitly for byte comparisons.

send copies accepted buffer/view bytes before queueing. A Blob argument is not
a byte-preserving send primitive merely because it stringifies: the locked
behavior sends `[object Blob]`. Typed-array slice offsets, source mutation,
conversion count/order, Symbols and argument getter failures have independent
observations.

The pinned send string and close-reason conversions differ for unmatched UTF-16
surrogates: send can produce WTF-8 (`ed a0 80` for `\ud800`), while the USVString
close reason produces U+FFFD. A validating network client can disconnect at that
text frame. These raw observations do not make invalid UTF-8 standard-compliant.
Capsid's selected incoming malformed-text cases follow the reference's
replacement-character behavior; Close-reason UTF-8 has an independent 1007 gate.

Public EventTarget/listener/handler attributes follow the declared workerd
default profile. Throwing listeners, removal/re-addition, nested dispatch,
option reads, signal removal and ordinary/attribute listener identity have
separate controls. Arbitrary diagnostic text is not broadly normalized.

## Close and Lifetime

The locked legacy JS close-code set is 1000–4999 except 1004/1005/1006/1015;
flags/dates such as pedanticWpt can differ. Close reason has its declared byte
limit. Empty Close and transport failure are distinct; close while CONNECTING
cancels the pending operation.

Remote EOF without Close reports code 1006, wasClean=false and
`WebSocket disconnected without sending Close frame.`. The callback observes
OPEN; the next microtask/timer observes CLOSING. Operations on disposed
transports still perform required argument and origin checks. Clean Close,
establishment failure, protocol error and resource denial remain distinct.

Ordinary local Close ends the sending direction without discarding legal
opposite-direction assembly/delivery. A final allowed message from an
allowHalfOpen callback precedes reciprocal Close. CANCEL/transport terminal
barriers instead stop producers and suppress invalid late delivery. The Host
must drain actual accepted writes and stop the socket before ACK; an internal
phase notification is not an ACK/queue-publication proof.

An unfinished outbound constructor follows its HTTP owner through the response
body's end. Held success/rejection/stall controls retain the JS objects, prove
actual peer closure and require no later callbacks; GC is not a cancellation
substitute. Returning a Response object alone does not end a streamed body's
owner. Once its open event begins, an established socket can continue after the
HTTP body. A different request cannot reuse that native I/O origin, even when
the object remains reachable or closed.

Retained local incoming owners after transport EOF use the optional v4 lifetime
extension. Physical network reservations and local async liveness are separate,
bounded counts; managed generation drain deadlines still apply. Read the
[wire/ACK contract](fetchrpc-v4.md), [embedding guide](embedding-api.md) and
[native stream lifetime](runtime-behavior.md#stream-observation).

## Outbound ws/wss

The global constructor uses Host-mediated DNS, address/egress checking,
TCP/TLS/SNI, Upgrade handshake and result barriers. Local fixture grants and
trusted CAs are explicit. Tests include invalid URL/protocols, pending cancel,
out-of-order results, coalesced handshake/data, large/fragmented messages,
invalid Accept, HTTP non-101/redirect, unknown CA and wrong hostname.

Before connection the URL retains its input and protocol/extensions are empty;
when none is selected the locked success observation uses null. Establishment
failure reports the case-specific Error/TypeError then terminal 1006 while
closed. Error objects and diagnostics are checked rather than being silently
replaced with a common string.

The reference accepts a server-selected protocol absent from the offered list;
Capsid rejects that under RFC 6455 section 4.1. This is an explicit product
security difference, not a missing comparison. Pending/result and channel/queue
budgets are finite. A constructor that cannot synchronously admit OPEN throws
rather than retaining an unannounced actor.

## Transport Scope and Limits

- Host-owned framing, masking, fragmentation and Ping/Pong are independent of
  JavaScript event semantics. Pings never become FetchRPC frames.
- Pong coalescing is legal. Ordered actual Ping payloads and interleaved data
  remain checked; a bounded observation ending its socket does not prove a
  watchdog killed a process.
- Capsid rejects invalid received Close codes with 1002 instead of reproducing
  the reference's selected invalid-code echo. Malformed Close-reason UTF-8 is
  checked separately.
- Default message and pending-send budgets are finite: Config v2 defaults to a
  1 MiB max message size with a 4 MiB worker-wide `send_budget_bytes` and
  2 MiB `per_channel_queue_bytes` (raw C ABI `ChannelLimits` defaults are
  1 MiB max message, 16 MiB worker-wide, and 4 MiB per-channel). A synchronous 8 MiB
  burst is resource-policy denial, not an unbounded retry queue. The reference
  default message limit is different.
- Compression, hibernation, raw listeners and cross-worker broadcast are not
  promised. Two-worker fanout probes prove room/instance isolation and native
  request ownership; they do not create a broadcast service.
- A precisely captured reference-only `Network connection lost.` ErrorEvent
  before a matching clean Close has its own exact allowance/count. Additional
  or altered errors, native errors or arbitrary ordering do not inherit it.

All stated differences remain visible with their source/date/flag scope. The
same code can be compatible with the selected reference and non-conformant to
a separate Web standard requirement.

## Standards, Qualification and Remaining Scope

The current [v2 workerd/WPT profile](../../tests/wpt/workerd-default-profile-v2.json)
retains five raw standard-file failures, including exact nested-task timeout
conflicts. MessagePort's initially disabled standard queue, the workerd event
profile and Headers differences are documented in [conformance](web-conformance.md).
Ordinary suite success and full standards conformance remain false; the older
v1/two-file classification is not the current release policy.

The declared `0.3.x` beta full/native/sanitizer/fuzz/soak/package gates passed; see
[current status](release-status.md), the [v0.3.2 verification record](../history/v0.3.2/README.md),
and the earlier [v0.3.0 qualification](../history/v0.3.0/release-qualification.md). This does not certify every
possible lifecycle/proxy graph, resolve missing historical schedules, grant
native macOS/Windows production isolation or turn a reference observation into
a universal specification. No new runtime/reference execution is claimed merely
because this document was refreshed. Current commands are in the
[testing guide](../guides/run-tests.md).

## Historical source and preserved anchors

Read the dated checkpoints and their recorded measurements in the
[English journal](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt). They are not current release instructions.

<details>
<summary>Legacy section links</summary>

<a id="workerd-websocket-semantics-dossier-v030-reference"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L1) — workerd-websocket-semantics-dossier-v030-reference

<a id="capsid-review-phase-107"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L8) — capsid-review-phase-107

<a id="archived-section-207"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L10) — Round 107 (2026-11-07): SDK70 and branch fixes merged, continued acceptance of mainline and clean builds

<a id="capsid-review-phase-106"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L31) — capsid-review-phase-106

<a id="archived-section-213"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L33) — Round 106 (2026-11-07): Progress in verifying private contexts and unread gzip branches

<a id="capsid-review-phase-105"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L47) — capsid-review-phase-105

<a id="archived-section-219"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L49) — Update #105 (2026-11-07)

<a id="capsid-review-phase-104-in-progress"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L59) — capsid-review-phase-104-in-progress

<a id="archived-section-225"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L61) — Round 104 (2026-10-24): 480 targeted combinations in working tree, 4 C API helper response-credit fixes, and runtime timeout isolation

<a id="archived-section-228"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L67) — Working tree merged: 4 C API test helpers return response credit

<a id="archived-section-231"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L87) — New discovery: The snapshot text becomes corrupted after some transmissions due to timeouts or cancellations, leading to runtime errors.

<a id="archived-section-234"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L97) — The private JS v7 is still retained within its original scope.

<a id="archived-section-237"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L103) — Current differences and continuation order

<a id="capsid-review-phase-103"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L112) — capsid-review-phase-103

<a id="archived-section-243"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L114) — Round 103 (2026-10-21): BYOB parameter order, standard capacity rejection, and ABI self-check synchronization

<a id="capsid-review-phase-102"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L143) — capsid-review-phase-102

<a id="archived-section-249"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L145) — Round 102 (2026-10-21): Large volume reading, DefaultReader allocation, and EOF offset

<a id="capsid-review-phase-101"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L176) — capsid-review-phase-101

<a id="archived-section-255"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L178) — Round 101 (2026-10-21): Reader prototype borrowing, Float16Array, and ABI test fixes

<a id="capsid-review-phase-100"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L207) — capsid-review-phase-100

<a id="archived-section-261"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L209) — Round 100 (2026-10-21): Upgrade and rollback of the native BYOB base matrix for HTTP response bodies

<a id="capsid-review-phase-99"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L238) — capsid-review-phase-99

<a id="archived-section-267"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L240) — Round 99 (2026-10-20): Direct DefaultReader construction path for native body

<a id="capsid-review-phase-98"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L267) — capsid-review-phase-98

<a id="archived-section-273"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L269) — Round 98 (2026-10-20): Optimizing real-time allocation probes and complete spawn exception traversal in the build process

<a id="capsid-review-phase-97"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L292) — capsid-review-phase-97

<a id="archived-section-279"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L294) — Round 97 (2026-10-20): Custom configurations, v4 initialization, and actual resource recycling

<a id="capsid-review-phase-96"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L325) — capsid-review-phase-96

<a id="archived-section-285"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L327) — Round 96 (2026-10-20): Default ABI allocation failure, resource cleanup, and test coverage fixes

<a id="capsid-review-phase-95"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L339) — capsid-review-phase-95

<a id="capsid-review-phase-94"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L353) — capsid-review-phase-94

<a id="archived-section-294"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L355) — Round 94 (2026-10-20): Default coding stack fixes, complete three configuration baseline, and new differences added

<a id="capsid-review-phase-93"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L374) — capsid-review-phase-93

<a id="archived-section-300"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L376) — Round 93 (2026-10-20): Five-stack QPS controller and archive connected

<a id="capsid-review-phase-92"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L387) — capsid-review-phase-92

<a id="archived-section-306"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L389) — Round 92 Status (2026-10-20): QPS load correctness and original-evidence fixes; full producer continues

<a id="capsid-review-phase-91"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L398) — capsid-review-phase-91

<a id="archived-section-312"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L399) — Round 91 Status (2026-10-20): Bytecode compiler mount-compatibility fixes; full lane targets continue

<a id="capsid-review-phase-90"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L408) — capsid-review-phase-90

<a id="archived-section-318"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L409) — Round 90 Status (2026-10-20): Cold-start controller and archiving enabled; full targets continue

<a id="capsid-review-phase-89"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L422) — capsid-review-phase-89

<a id="archived-section-324"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L423) — Round 89 Status (2026-10-20): Cold probe fixed; full lane with strict targets not yet completed

<a id="capsid-review-phase-88"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L434) — capsid-review-phase-88

<a id="archived-section-330"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L435) — Round 88 Status (2026-10-20): Density control and archive producer are connected

<a id="capsid-review-phase-87"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L446) — capsid-review-phase-87

<a id="archived-section-336"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L447) — Round 87 Status (2026-10-20): Corrected performance collection and web evidence boundaries

<a id="capsid-review-phase-86"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L458) — capsid-review-phase-86

<a id="archived-section-342"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L459) — Round 86 Status (2026-10-20): Completed controlled build comparison for Windows stack pressure

<a id="capsid-review-phase-85"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L470) — capsid-review-phase-85

<a id="archived-section-348"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L471) — Round 85 Status (2026-10-20): 30 new transform and lifecycle scenarios completed and verified

<a id="capsid-review-phase-84"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L482) — capsid-review-phase-84

<a id="archived-section-354"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L483) — Round 84 Status (2026-10-20): Outbound fetch boundary additions completed and verified

<a id="capsid-review-phase-83"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L494) — capsid-review-phase-83

<a id="archived-section-360"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L495) — Round 83: Fetched Response Native Coupling Verified Semantics (2026-10-20)

<a id="capsid-review-phase-82"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L504) — capsid-review-phase-82

<a id="archived-section-366"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L505) — Round 82 Status (2026-10-20): Managed update retained local connection sources, fixed Windows incremental caching

<a id="capsid-review-phase-81"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L516) — capsid-review-phase-81

<a id="archived-section-372"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L517) — Round 81 Status (2026-10-20): Local close-delay gap resolved

<a id="capsid-review-phase-80"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L526) — capsid-review-phase-80

<a id="archived-section-378"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L527) — Round 80 Status (2026-10-20): Abnormal parent-connection pending events and local Close

<a id="capsid-review-phase-79"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L534) — capsid-review-phase-79

<a id="archived-section-384"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L535) — Round 79 Status (2026-10-20): Shared I/O lifecycle and complete mandatory gate

<a id="capsid-review-phase-78"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L544) — capsid-review-phase-78

<a id="archived-section-390"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L545) — Round 78 Status (2026-10-20): 13 public Upgrade-fetch baseline scenarios passed

<a id="capsid-review-phase-77"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L552) — capsid-review-phase-77

<a id="archived-section-396"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L553) — Round 77 (2026-10-20): Upgrade-fetch foundation verified; public path pending wiring

<a id="capsid-review-phase-76"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L562) — capsid-review-phase-76

<a id="archived-section-402"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L563) — Current progress of the 76th round (2026-10-19): Collection of built-in methods, fetch request headers, and current full regression tests

<a id="capsid-review-phase-75"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L576) — capsid-review-phase-75

<a id="archived-section-408"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L577) — Round 75 (2026-10-19): WebCrypto digest/HMAC and full regression verification

<a id="workerd-websocket-semantics-dossier-v030-reference-1"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L588) — workerd-websocket-semantics-dossier-v030-reference-1

<a id="capsid-review-phase-74"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L591) — capsid-review-phase-74

<a id="archived-section-417"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L592) — Current progress of round 74 (2026-10-19): Timeout abort bridge resumes request context

<a id="capsid-review-phase-73"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L601) — capsid-review-phase-73

<a id="archived-section-423"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L602) — Current progress of round 73 (2026-10-19): fetch caller context restoration

<a id="capsid-review-phase-72"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L611) — capsid-review-phase-72

<a id="archived-section-429"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L612) — Current progress of the 72nd round (2026-10-19): fetch resource recycling and cancellation semantics

<a id="capsid-review-phase-71"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L621) — capsid-review-phase-71

<a id="archived-section-435"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L622) — Current progress of the 71st round (2026-10-19): Cross-context Promise

<a id="capsid-review-phase-70"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L631) — capsid-review-phase-70

<a id="archived-section-441"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L632) — Current progress of the 70th round (2026-10-19): Asynchronous callbacks and execution budget

<a id="capsid-review-phase-69"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L641) — capsid-review-phase-69

<a id="archived-section-447"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L642) — Current progress of the 69th round (2026-10-19): Connecting timers with Promise lifecycle

<a id="capsid-review-phase-68"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L675) — capsid-review-phase-68

<a id="archived-section-453"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L676) — Current progress of round 68 (2026-10-19): Native exception reconstruction and built-in string conversion

<a id="capsid-review-phase-67"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L709) — capsid-review-phase-67

<a id="archived-section-459"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L710) — Round 67 (2026-10-19): Cloning of plain Error objects and engine type checks

<a id="capsid-review-phase-66"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L743) — capsid-review-phase-66

<a id="archived-section-465"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L744) — Current progress of the 66th round (2026-10-19): I/O for exception getters and error listeners

<a id="capsid-review-phase-65"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L776) — capsid-review-phase-65

<a id="current-checkpoint-handler15b-synchronous-exception-snapshots"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L777) — current-checkpoint-handler15b-synchronous-exception-snapshots

<a id="capsid-review-phase-64"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L790) — capsid-review-phase-64

<a id="current-checkpoint-handler14i-accepted-writes-and-trusted-errors"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L791) — current-checkpoint-handler14i-accepted-writes-and-trusted-errors

<a id="capsid-review-phase-63"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L804) — capsid-review-phase-63

<a id="current-checkpoint-accepted-frames-before-a-synchronous-throw"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L805) — current-checkpoint-accepted-frames-before-a-synchronous-throw

<a id="pair-endpoint-transfer-and-accept"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L860) — pair-endpoint-transfer-and-accept

<a id="events-binary-values-and-send"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L879) — events-binary-values-and-send

<a id="close-and-pending-connection-cancellation"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L919) — close-and-pending-connection-cancellation

<a id="transport-observations-and-remaining-acceptance"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L959) — transport-observations-and-remaining-acceptance

<a id="implemented-inbound-scope-and-explicit-deviations-2026-10-16"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L994) — implemented-inbound-scope-and-explicit-deviations-2026-10-16

<a id="outbound-constructor-observations-2026-10-13-continuation"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L1037) — outbound-constructor-observations-2026-10-13-continuation

<a id="additional-compatibility-observations-2026-10-15"></a>
[Historical section](../history/archives/2026-11-17/workerd-websocket-semantics.md.txt#L1093) — additional-compatibility-observations-2026-10-15

</details>
