# Capsid v0.3.0 implementation design

<!-- capsid-historical-context-20261117 -->
> **Historical plan/review — not current execution instructions.** The original
> body, counts, failures and pending language below retain their captured source/date.
> Stable is v0.2.2 and experimental 0.3.0-beta is now published. Use
> [current status](../../reference/release-status.md) for delivered scope, exact qualification and
> measured-source data; do not restart old campaigns from this record.
<!-- /capsid-historical-context-20261117 -->

This plan is used to complete the FetchRPC v4 and WebSocket milestones, and to fix the ABI, process recycling, buffering, and performance issues identified by the review. The implementation sequence is as follows: [GLM Execution Plan](delivery-plan.md) The scope is as follows: [ROADMAP](../../../ROADMAP.md#v030-fetchrpc-v4-and-websocket-foundation) For qualification purposes.

The newly added interfaces, A2 protocol layout, and resource default values are the design inputs for this implementation. The GLM must first complete the corresponding contract testing, and write the final byte layout and error table. [FetchRPC Contract](../../reference/fetchrpc-v4.md) Implement the channel runtime. They have not yet become implemented capabilities. The existing v3 line format, released ABI v7 layout, and sandbox guarantees continue to be subject to regression testing.

## 1 Design Decisions

| Project | Implementation Plan |
| --- | --- |
| ABI | Maintain the original structure of v7, add independent protocol options and spawn entry points. The new descriptor has been implemented with two-way size negotiation since the first version |
| Protocol Amendment | Register the A2 amendment in v4, using explicit capability bits to distinguish the modified channel layout; the base load of v3 and v4 carriages remains compatible |
| Channel Addressing | Each worker IPC connection is independently assigned a u32 channel ID by the host authority; the channel frame explicitly carries the ID. |
| Outbound Association | The worker is assigned a u64 dial ID, and the host returns the assigned channel ID; it also carries a URL and a list of subprotocols. |
| Ownership | 101 When submitting, transfer resources from the handshake request to the connection owner; HTTP response cleanup does not release active connections. |
| Network | Host handles HTTP Upgrade, RFC 6455, DNS, and TLS; Worker only handles JS messages and FetchRPC channels |
| Flow control | Two-way byte credits; transmission queues, message reassembly, and JS pending queues are billed separately and are subject to the total budget constraints of workers and hosts. |
| JS Behavior | Determined by locking the workerd source code and actual fixtures; Resource constraints of Capsid are recorded in the conformance deviation |
| Measurement | Repair the lanes and collectors to regenerate the points; each number retains the source, posture, original sample, and verifiable summary |

This version does not implement hibernation, compression extension, Binding RPC v2, `ctx.waitUntil()`, Wrangler workflows, or QuickJS lazy compilation. Whether the standard WebSocket `accept({ allowHalfOpen })` is supported and its default behavior are determined by the lock reference test, and it cannot be excluded directly after hibernation.

## 2 ABI v7 and configuration entry points

### 2.1 Restoring the Published Structure

Restoring the fields, order, size, and alignment of `capsid_worker_config` from the `aef6020` snapshot corresponding to the `v0.2.2` tag, removing the added `protocol_version`, `protocol_reserved`, and `protocol_capabilities` at the end. Restoring the original snapshot of `tests/compat/runtime_v7.h`; it cannot be extended with the new implementation.

The old `capsid_worker_config_init()` still initializes with the original v7 size. The old caller often passes in uninitialized structures, so it cannot be modified to read the `struct_size` that has not been defined, to pretend to be a compatibility fix. The old `capsid_worker_spawn()` always uses v3.

The existing `capsid_event`, which is a publicly enumerated set of values and function signatures, remains unchanged. V4 channel events are represented by newly added enumeration values and typed decoders; new behaviors are only triggered by explicitly selecting V4 as the entry point.

### 2.2 Independent Descriptor

The new interfaces adopt the following formats, and the final names and error mappings are synchronized in the ABI task:

```c
capsid_result capsid_protocol_options_init(
    capsid_protocol_options *options, uint32_t caller_size);

capsid_result capsid_worker_spawn_ex(
    const capsid_worker_config *config,
    const capsid_protocol_options *options,
    capsid_worker **out_worker);

capsid_result capsid_worker_get_protocol_info(
    const capsid_worker *worker,
    capsid_protocol_info *out_info, uint32_t caller_size);
```

`capsid_protocol_options` contains `struct_size`, descriptor version, wire version, reserved, requested capabilities, and an optional channel limits descriptor. `options == NULL` is equivalent to the old v3 entry. The descriptor version is separated from `CAPSID_ABI_VERSION`, with version 1 being the first version.

All new descriptors follow these rules:

- The initialization function obtains an explicit buffer size; if it is insufficient to accommodate a fixed prefix, a parameter error is returned, with no bounds violations written.
- Initialize to `min(caller_size, library_known_size)`; input parsing checks each field to ensure it is within `struct_size`.
- The parsing allows future callers to provide a larger structure, but only reads fields from the known version; explicitly rejecting unknown descriptor versions.
- The content of the nullable pointer is synchronously copied into an immutable snapshot during spawn, and subsequent calls do not borrow the temporary memory of the caller.
- Unknown capabilities, dependencies not satisfied, reserved non-zero values, and illegal limits are rejected before creating a socketpair, child process, cgroup, or Windows handle.
- READY. Before this state, the protocol info is queried, and `CAPSID_WOULD_BLOCK` is returned. After reaching the READY state, the wire version, the capabilities of both parties, and the effective intersection are returned.

The host internal and C++11 wrapper use new entry points. The C11 header, C++11 wrapper, original v7 consumer, smaller new descriptors, and larger future descriptors all require separate linking and sentinel testing.

### 2.3 Launch Resource Recycling

Manage unpublished processes, IPC FDs, Windows process/thread handles, and temporary cgroup states through RAII or equivalent single owner mechanisms. Ownership is transferred only upon successful return of the worker. Any failure or exception results in the closure of the FD, termination, and recycling of the actual child process. Semi-finished products without recorded PID should not be handed over to `capsid_worker_destroy()`.

## 3 FetchRPC v4 A2 Amendment

### 3.1 Versions and Capabilities

The frame header retains 24 bytes. The magic number is `0x43545257`, and the LE bytes are `57 52 54 43`. Do not replace the gold bytes with incorrect ASCII annotations. The definitions for types 1 to 18 in v3 are retained, and types 19 to 26 are not reallocated.

Register `CAP_CHANNEL_LAYOUT_A2 = 1u << 3`. Duplex, Upgrade, and HalfClose all require A2; Upgrade and HalfClose also require Duplex. Combinations with only A2 and no Duplex will be rejected. Unknown bits and reserved non-zero values are still considered fatal protocol errors.

When neither party declares channel capabilities, the v4 carriage uses the existing Hello/READY format. The party enabling A2 uses a clearly modified payload; the old v4 parser will reject unknown bits and must not misinterpret the new layout. Valid functions are determined by the intersection of both parties' capabilities, and dependency closures are verified. Frames with disabled functions are rejected at the state decoding layer, and it is not sufficient to only check the frame number for validity.

Worker bootstrap allows only the first frame to be HELLO, with the wire version being 3 or 4; the version is locked after the entire frame is successfully verified. Any subsequent frames across versions are rejected, and there is no downgrade within the connection. Frames that are not HELLO, repeated HELLO, bad READY, and false capability intersections all result in a definite error.

### 3.2 Hello and READY

v4 carriage adds an additional 8 bytes of capability tail to the fully valid v3 startup payload. A2, on the other hand, appends a length marker extension between the startup payload and the capability tail.

```text
v3 startup payload
u32 extension_size = 48 Only declared when A2
u16 extension_version = 1
u16 reserved = 0
u32 max_channels
u64 max_message_bytes
u64 reassembly_budget_bytes
u64 send_budget_bytes
u32 close_timeout_ms
u32 dial_timeout_ms
u32 idle_timeout_ms
u32 per_channel_queue_bytes
u32 capabilities
u32 reserved = 0
```

All fields are marked with LE. Extension version/size, arithmetic overflow, all budget relationships, and trailing bytes must be verified. The cursor of the original v3 decoder is resolved after startup ends; a claim of a valid HELLO cannot be generated from a zero-filled array.

The READY wire payload continues to add additional capabilities after being qualified and proven in the sandbox. The client first verifies the capabilities and saves the typed protocol information, then passes the READY payload without the added capabilities to the original sandbox verification process. The checks for binding proof's digest, seccomp, Landlock, and namespace remain strong even in version 4. The description of the C ABI's READY payload is updated synchronously, and the wire negotiation is verified by raw peer tests.

### 3.3 Channel Load

The `request_id` in all channel headers is reserved as 0; the channel ID must be obtained from the payload. There are separate fields for the owner request ID and dial ID. IDs are incremented from 1 within the current IPC connection and are not reused. New channels are rejected when resources are exhausted, and workers are replaced in a orderly manner by the supervisor.

The input for the A2 implementation layout is as follows. The qualification task requires completing the enumerated values, length limits, directions, and error strings into the standard table, updating the vectors schema, and generating independent decoding tests:

| Frame | Load |
| --- | --- |
| OPEN 19 | `u32 channel_id, u64 dial_id, u8 owner_kind, u8 channel_kind, u16 reserved, u64 owner_request_id, u64 max_message_bytes, u32 url_size, url bytes, u16 protocol_count, length-prefixed protocols` |
| ACCEPT 20 | `u64 dial_id, u32 channel_id, u64 accepted_max_message_bytes, length-prefixed selected_protocol` |
| REJECT 21 | `u64 dial_id, u32 channel_id, u32 reason_code, length-prefixed bounded diagnostic` |
| DATA 22 | `u32 channel_id, message fragment bytes`; flags are start/end/text |
| CREDIT 23 | `u32 channel_id, u64 credit` |
| HALF_CLOSE 24 | `u32 channel_id` |
| CLOSE 25 | `u32 channel_id, u8 close_kind, u8 flags, u16 code, u16 reason_size, reason bytes` |
| CANCEL 26 | `u32 channel_id, u32 terminal_reason`; In CONFIRM, do not allocate dial usage with `channel_id=0`, and add `u64 dial_id`; if the host rejects, still use REJECT. |

Worker outbound OPEN is for `channel_id=0`, non-zero dial ID, `ws:`/`wss:` URLs, and candidate protocols. After assigning a host ID, it is ACCEPT; if failed, it is REJECT. Inbound passes through Upgrade head reservation; no active OPEN is performed using the host. The protocol string uses a u16 byte length, URLs use a u32 byte length, and diagnostics use a u16 byte length; the total payload must still not exceed 64 KiB.

2026-10-13 Supplement: Channel IDs are assigned monotonically in the order declared by the wire. Dial IDs are independently assigned as monotonous u64 values and are not reused. The T05 v4.2 revision uses the ability bit 4 `CAP_CHANNEL_CONFIRM` and ACK 27 (`u32 channel_id, u64 dial_id, u32 kind`), adding a result barrier for ACCEPT/REJECT and a terminal barrier for cancellation/transport Close. DATA cannot be generated before confirmation by ACCEPT; after cancellation, the queued results/data are only verified and emptied, without being delivered to JS. Bound associations are reclaimed upon confirmation or expiration, preventing misjudgment of late ACKs due to lost associations. Double-end FIFO playback and real IPC raw-peer overlap with full queue retries. This inbound connection has already connected JS/network/owner, and workers declare 27 (DUPLEX/UPGRADE/A2/CONFIRM) when negotiating CONFIRM, without declaring HALF_CLOSE. The reception correction for normal Close has been updated to v4.2.1, with the byte layout remaining unchanged. See the complete rules for details. [wire contract §3.5.1](../../reference/fetchrpc-v4.md#351-confirmation-amendment-v42-2026-10-13).

The maximum number of valid message bytes for DATA is two `65536 - 4 = 65532` Credit is only charged based on the number of valid bytes in the message. The text attribute of the “flags” field cannot be changed within a single message. A single message is transmitted at the same time start/end, Multiple messages only include the first one start, Final piece end.

The "Upgrade" packet no longer sends additional OPEN messages: The upgrade header of the RequestHead contains `u32 channel_id, u64 max_message_bytes`; the upgrade header of the ResponseHead with code 101 returns the same ID and the maximum message size accepted. A successful ResponseHead marks the submission point transitioning from pending to open. Non-101 responses release the pending reservation, without leaving any channels or generating DATA.

101 Upgrade is mutually exclusive with the fixed-body flag. The selected subprotocol is extracted from the worker response header. The host must confirm that it belongs to the request list and can only select one. Connection, Upgrade, and Sec-WebSocket-Accept are generated by the host. The upgrade response does not send a regular ResponseEnd; the typed upgrade event explicitly ends the HTTP request accounting and establishes a connection accounting.

### 3.4 Errors, Closures, and Contradictions

CLOSE has different close kinds: initiated Close, observed peer Close, and transport terminated. The final close kind value and flag matrix are frozen during the contract task. The code-present flag allows an empty Close body to be expressed; 1005/1006/1015 cannot be used as normal sending status codes. 1006 can only be used for abnormal termination observations and must not be encoded as a network Close frame.

The triggering points of JS CloseEvent, whether `wasClean` and whether an automatic response is given after receiving a Close command are determined by the workerd reference fixture. The recycling of transport is notified separately, and the term "JS event dispatched" should not be interpreted as "all writes completed". Errors are explicitly represented through REJECT, CANCEL, or the original kError. The contract must have error mappings for channel/request/connection, and silently swallowing errors is prohibited.

Also, Close is a valid bidirectional action. There are clear results for repeated identical Close operations, repeated OPEN operations, unknown active IDs, data out-of-order, credit overflow, and DATA without credit. Repetition CREDIT is an additional increase in quota, not considered an identity operation. Internal cleanup operations are idempotent, but the repetition of wire frames is still determined by the contract. The limited tombstone only handles late completions that can be proven to come from completed asynchronous callbacks, and does not exempt malicious wire input.

## 4 Channel Status and Resource Ownership

It is recommended to use the project's own `channel_registry` and `channel_state` modules to carry common logic, to avoid duplicating similar but inconsistent state machines in both the host and worker.

```text
reserved / dialing -> pending -> open -> closing -> closed
                          \-> rejected
open: send direction and receive direction have open/half-closed states respectively
```

| owner | Lifecycle |
| --- | --- |
| handshake request | 101 Submission timeout before request and connection interruption constraints |
| duplex connection | After submission, the inbound 101 process is independent of the HTTP response; it remains bound to a specific worker generation. |
| request | The ordinary request-bound channel terminates with the request. |
| response stream | Terminate with the response stream |
| standalone | The outbound connection is independent of the HTTP response that initiates it, but is subject to constraints on workers, generation, explicit close, and budget. |
| tail task | Reserve value; v0.3.0 explicitly refuses creation |

The resource always records the immutable User/Binding authority. The lifetime owner is separated from the permission origin, and the authority cannot be expanded when transferred to the connection owner. The outbound socket created by the User does not inherit the Binding identity.

The worker creates a separate async owner/token for handling connection events. The message/close/error received after 101 does not borrow the already used HTTP request token, and does not provide any additional tasks for the user. The message callbacks and their Promises are still subject to constraints related to CPU usage, heap space, and task scheduling limitations.

Client disconnection, request handshake timeout, dial timeout, close timeout, worker exit, and generation replacement all fall under the same cleanup process. The cleanup sequence is as follows: stop the generation of new events, cancel unfinished IO operations, terminate pending promises, complete or interrupt restricted writes, revoke token/native roots, and release quotas and pool references. Each channel can only be sent a single JS terminal close request; all native owners will eventually be released.

The managed blue-green transition stops the old generation from accepting new connections, and sends a 1001 to its existing WebSocket with a bounded drain. Upon the expiration of the period, it is forced to be reclaimed. Connections cannot be migrated to the new worker, and the old socket callbacks cannot be delivered to the alternative process.

## 5 Backpressure and Memory Budget

Default values are configured for the first implementation and should not be considered as performance conclusions:

| Resources | Initial Setup | Constraints |
| --- | --- | --- |
| Two-way initial credit | Each is 64 KiB in size. | Independent u64; Addition and subtraction validation for overflow/subtraction |
| Each channel transmission queue | Each direction can have a maximum of 4 MiB. | Also subject to the overall budget constraints of worker/host. |
| Maximum number of single messages | 1 MiB | The two parties will choose the smaller value; hard limit is 64 MiB |
| Reorganize the total budget for worker messages | 16 MiB | Reserve before creating the reorganization area; does not increase infinitely with the number of channels |
| Total budget for pending messages of worker | 16 MiB | Both JS view/native copies are billed, and external memory is also counted. |
| Number of channels per worker | 128 | Count separately from HTTP inflight, and the deployment can be limited downward. |
| Dialing period | 10 seconds | Including analysis, TCP, TLS, HTTP handshake |
| Close Drain Deadline | 5 seconds | Interrupt IO upon expiration and release the owner |
| idle period | Default 0, disabled | Long connection does not inherit from 30 seconds HTTP request timeout |

To increase the message limit, it is necessary to satisfy both the host, worker, JS heap, and reorganization budget. Reserve a quota before each allocation and return it when it is released; return a reproducible error or bounded closure when exceeding the limit, and do not silently discard messages. The host also limits all connections according to the total budget of the listener/entire service, so it is not possible to prove the limitations of a single channel alone.

2026-10-14 Handoff Implementation: The service instance has a `ServiceChannelBudget`, which is static pool
shard, Managed active/draining/replacement are shared; inbound listeners use the service-based approach
The sub-budget of the parent. See the operator's three `capacity` fields and two listener fields.
[Configuration reference](../../reference/host-configuration.md#websocket-aggregate-limits-and-metrics) All values are positive.
Default service: 1024 channels / 128 pending dials / 16MiB network write buffer, listeners
128 channels / 16MiB. The budget object remains on Host, with existing Worker channel limits continuing
Affecting generation identity, service aggregate ceilings do not enter wire/ABI.
Standalone User outbound does not have a listener owner and is accounted for separately in the service budget.

Reserved connections, active connections after 101/result ACK, pending dial, and HTTP inflight counts are recorded separately.
The buffer lease is released with the last asynchronous write owner, and cannot be released earlier than the session teardown.
Return. Prometheus exports used, limit, and high-water values for fixed scope/listener/resource tags.
And rejection count; not interpreting the network write buffer limit as the entire Host/Worker's memory budget.

The network codec has added an interface for handling incremental payload chunks. When a large WebSocket frame is received, only a limited number of header/scratch tags and fragments are retained. These fragments are mapped to FetchRPC DATA. It is not allowed to process frames that exceed 64 MiB in size, as they must be split into smaller IPC fragments. The original Frame API can be used as a test adapter for handling smaller fragments.

Return credit once the downstream receiver accepts a fragment into its reserved reassembly buffer or the network write completes. Data received but not yet accepted earns no credit. Dispatch to JS may transfer ownership of a complete message within the budget. Credit must be replenished before the entire message is complete or messages larger than 64 KiB will stall.

`WebSocket.send()` maintains the same return type as the reference. Native pumps can use WOULD_BLOCK and pending promises, but the JS send method has not been updated to a new API that can be awaited. When the send queue reaches its limit, a deterministic exception or termination strategy registered in the conformance is adopted, while maintaining an accurate buffered amount. No commitment is made to "final blocking or discarding".

## 6 Host Data Paths and Outbound Strategies

Single-worker, static-pool, and Linux managed share the handshake checksum and WebSocket session logic. The responsibilities for HTTP status, routing, CORS, and admission are not replicated across the three modes.

The handshake identifies the WebSocket Upgrade, then verifies GET, HTTP/1.1, Connection token, Upgrade token, a unique and decodable 16-byte key, version 13, and subprotocol syntax. Sec-WebSocket fields that are irrelevant for regular HTTP do not trigger handshake errors. Repeated Connection/Upgrade is handled by merging HTTP fields and token rules, and if a duplicate is encountered, kNone is returned to bypass the verification. Failure is indicated by returning 400/426 before requesting a worker request/channel slot.

101 Switch to retaining the first WebSocket frame byte read in the Beast buffer, and enable the WS write pump after serializing the HTTP handshake write. The connection detection switches from HTTP to WS transmission ownership. The ping/pong, Close, and cancellation of control frames have independent limited progress. They cannot be starved by zero DATA credits, nor can they be set to an infinite control queue for ping floods.

The outbound connections are handled by the host dialer, using the App egress/capability snapshot saved during spawn:

1. Verify the ws/wss URL, port, fragment/credentials, and candidate protocols.
2. Perform a policy check on the hostname and the actually resolved address in sequence, maintaining the existing semantics where explicit IP/CIDR deny, protected ranges, and hostname authorization take precedence.
3. Connecting to authorized parse results; TLS execution CA, hostname/SNI verification.
4. Verify 101, Upgrade/Connection, Sec-WebSocket-Accept, and the selected protocol; if failed, return REJECT with the dial ID.
5. Pump network messages and FetchRPC channels into each other, stopping resolver, connect, TLS, and handshake simultaneously when canceling.

The initial scheme does not follow WebSocket HTTP redirection; refer to the fixture if it is required to support this. First, record its authorization and re-parse rules, then modify the protocol and conduct tests. It is prohibited to reuse addresses that have not been verified for current authorization. The txiki ws.c, raw socket, and unauthorized Binding modules within the worker remain closed to the User.

## 7 JS API and Embedded Interfaces

A new project file `js/websocket.js` has been added. It connects to the controlled native channel through bootstrap, without directly exposing the txiki WebSocket. The 101 extension of the Response only accepts the transferable client endpoint of the current pair, with the body being empty. The same endpoint that has been accepted or transferred cannot be transferred again.

First, lock down workerd to confirm: repeated acceptance, incorrect use of endpoints, text/binary types, binaryType, send parameter conversions, close code/reason, allowHalfOpen, exceptional events, and readyState. Valid usage involves creating pairs, accepting servers, and returning clients in 101 responses. Whether the ability to use same-isolate pairs is retained depends on the differential results and existing deviation records; it is not possible to claim full compatibility with Object.values using a pseudo-client object.

The new responsibilities of the public C ABI include: submitting an Upgrade and reserving a channel ID, receiving upgrade decisions, responding to worker outbound dials, sending messages, granting credits, half-closing, Closing, and Canceling. Channel events are sorted along the existing event pump, using the newly added event kind and decoder. The decoder outputs ID, dial ID, owner, u64 credit, and borrowed payload, with an explicit caller_size to protect the new structure.

The API signature table must specify the call direction, valid status, whether the input has been consumed when WOULD_BLOCK, the payload validity period, reentrance/thread rules, and whether a cancellation is required after a failure. The first version does not expand the size of capsid_event. If the draft name of `capsid_worker_upgrade_respond` does not match the actual direction of host/worker, use the above responsibilities splitting to replace and update the contract.

## 8 Verification and Release of Evidence

Wire grammar, payload decoder, capability gate, and channel state machine are verified in a layered manner. The golden sequence includes actual frames, driving directions, initial credits, state/events at each step, and explicit errors. Do not count the number of steps as part of the state machine testing. The runner must first perform independent absolute assertions, and then compare the results of two runs.

0.3.0 Release Gate Requirement: The WS scenario must be actually registered and executed. WebSocketPair that is not yet public, lacks drivers, exit 77, or ranNothing must not be considered completed. The Linux privilege skip instructions for general development environments are retained. Formal Linux production gate must be executed in an environment that meets the prerequisites.

The testing coverage includes: original v7 callers, v3/v4 mixed streams, size negotiation, no resources after rejection, multiple dials with disordered completion, messages larger than the initial window, fragmentation, slow consumers, control frame insertion, simultaneous closes, half-closes, timeouts, disconnections, shutdowns, worker replacement, and strict egress/TLS rejections. ASan/UBSan, host TSan, v4/state/handshake fuzz, and live-channel soak all require actual coverage of the new paths.

Performance work is divided into lane repair and data retesting. Lane repair requirements include: explicit recording of production states and verification during release collection; density ports allocated at the control layer; sampling/cleaning of recursive process trees; worker coverage evidence; clear labeling of cold/warm states; stable reads from both sides; failure of final verification does not result in publication of results. Collectors reject evidence lacking, bad summaries, identity conflicts, and duplicate cells. Record the origin of each lane, and measurements cannot be replaced by HEAD binary commits during collection.

The old raw artifacts remain unchanged. Old web page elements whose sources cannot be proven are marked as unverifiable or conclusively withdrawn, and no false summaries can be provided. The new density, QPS, and cold-start directories are included in the evidence, and they are generated by the same release binary and the same quiet session. The memory descriptions on static web pages are limited to the actual sampling window and workload.

Reference fact sources are: [Workerd Semantic Archive](../../reference/websocket-semantics.md) locked [WebSocket implementation](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/web-socket.c%2B%2B), [API Definition](https://github.com/cloudflare/workerd/blob/8f311ae3e3cf3064983f56a6a1c8b7ee33d6b99f/src/workerd/api/web-socket.h) and [RFC 6455](https://www.rfc-editor.org/rfc/rfc6455.html).

## 2026-10-13 Outbound Implementation Supplement

User-origin ws/wss has been implemented, see [Section 8 of the Handover Record](takeover-review.md#section-8).
The standalone owner should submit a complete OPEN before the constructor returns. Synchronize with RangeError when atomic insertion is not possible.
No host should remain invisible and without a protocol duration. ACCEPT has been observed, but the result is ACK/open.
The unfinished "close" should be changed to "assigned Cancel", and first complete the "result ACK". The Host is responsible for the "result ACK"
Do not start WS data before arrival; use User spawn snapshot and actual DNS sockaddr for authorization.
Perform CA/hostname/SNI verification. The Binding bridge still refuses.

The unrequested subprotocol of Reference, the pending HTTP owner, and the EOF readyState/diagnostic
For differences, see [Semantic Archive](../../reference/websocket-semantics.md) The green light for 43 items of development in the previous round does not replace anything.
Release the gate; the subsequent implementation of the DNS and generation lifecycle is described in the next section. production strict,
The full matrix of Parameters/ErrorEvent, Binding, and CPU/heap has not yet been qualified.


## 2026-10-14 DNS and Connection Lifecycle Supplement

The Host outbound no longer uses Asio's backend system `getaddrinfo` tasks. Each query is conducted independently.
c-ares channel, drives DNS sockets in dialer's io_context with 10ms non-blocking poll
And timeout settings; it is possible to cancel the immediate destruction of channels, sockets, and timers. Additionally, callbacks for results that are currently pending processing will also be canceled.
Revocable. The new query reads the system's DNS/hosts configuration, uses c-ares's parsing semantics, and does not load the OS.
Any NSS module. The overall service dialing duration is still controlled by the negotiated dial_timeout_ms.

`ARES_AI_NOSORT` must be enabled: [c-ares address sorting](https://c-ares.org/docs/ares_getaddrinfo.html)
Attempt to connect the return address; these connections must not be allowed to take precedence over the egress decision of resolved sockaddr.
The query returns up to 64 endpoints. The normal path is the same as before. Connect after authorization. TLS verification is as follows:
The result ACK barrier remains unchanged. Canceled [ares_destroy](https://c-ares.org/docs/ares_destroy.html)
Reclaim DNS resources without leaving any backend resolver tasks behind. This change does not introduce any new wires or public ABI changes.

For Host Networking with Boost, there is a new dependency requirement: c-ares >= 1.27. Static providers are given
priority. There is no need for configure-time downloads or blocking resolver fallbacks. The private generation
header is fixed to the selected provider, thereby preventing different versions of include prefixes from being introduced.
Instructions for installing dependencies on Linux/macOS/Windows have been updated along with the existing build processes. This change does not indicate that platform validation has
been carried out. Native DNS server overrides and stage observers are only available for native owners/testing purposes.
There are no JS/IPC/config fields available for providing pre-resolved addresses or for changing DNS authorities.

[DNS/dialer driver](../../../tests/test_host_dns_dialer.cc) Use real A/AAAA/NXDOMAIN,
Unresponsive DNS, Full TCP Backlog, Paused TLS/HTTP Peers, Verify Each Phase
Cancel/timeout/stop and provisional ACCEPT cancel; retain the owner beyond the original deadline for verification
No late results found. Check service slots/bytes and DNS fd/thread recycling. Linux connect wrapper
Verify that the parsing itself does not detect return addresses. Active session test results with ACK activation and close durations.

[generation driver](../../../tests/test_host_channel_lifecycle.cc) Using real workers/network:
The active or pending standalone channels after the HTTP terminal continue to be counted as generation.
liveness; retire - Reject new requests, existing connections continue to send and receive messages, new generations can serve requests.
The regular Close, drain deadline, SIGKILL, and health recycle mechanisms are all necessary for terminating old sockets and
freeing up resources. The replacement mechanism also involves re-establishing the WS and ensuring that messages are sent and received successfully. This driver only verifies the shared pool/executor
path; it cannot be used in all scenarios involving Managed Administration, Binding, or platform-level integration scenarios.
