# Native runtime behavior

These implementation notes describe the source and regression controls reviewed for the November 2026 v0.3.0 beta. The three sections cover observable behavior, ownership and the tests that enforce each contract. [Release and source scope](release-status.md)

## Stream observation

An ordinary native fetch body can remain locked and have pending reads after its owning HTTP response ends. The upstream connection is retired with the request. The public read promises and stream locks retain the state observed in the pinned workerd profile.

A designated native stream future retains each subscriber's request identity without treating the parked observation as active continuation work. This includes internal default and BYOB read roots, native pipe waits, and async iterator reads that use the internal read algorithm directly. Ordinary application stream promises retain their existing accounting.

Native Promise forwarding and suspended async results propagate this designation only from an already designated pending native stream future. A late designation converts existing parked reactions under each original subscriber's context. Capture failure preserves the unconverted job lease, and conversion remains subject to the VM interrupt budget. Delivery still checks the subscriber's original scope. It does not grant authority to perform native I/O for another request.

Normal pending promises and timers remain subject to normal request lifetime checks. The regression controls retain the ordinary input, aggregate parent, and custom `then` handlers so that garbage collection cannot remove the work under examination. A read followed by a retained ordinary wait or timer still trips the existing terminal continuation guard. A completed ordinary wait or timer permits a clean exit.

The native tests use a real held raw or gzip body. They receive a three-byte decoded prefix and retire the actual owner response and upstream connection before submitting an observer request. The observer compares metadata, reader state, bytes, backing buffers, and locks with the pinned primary. Positive cases then complete a third healthy request in the same worker, account for every HTTP response head, terminal frame and returned byte credit, and observe a natural operating-system exit.

`native_retired_ownership_*` covers Promise adoption and combinators, tee and clone graphs, pipe and transform graphs, `values()` and `for await`, and ordinary-promise boundary controls. Each group has a 60-second CTest budget; the helper retains its five-second event budget and 2.5-second natural-exit budget. `native_retired_ownership_contract_controls` checks captured passing reports, actual old native failures, incomplete reports, coherent public-state corruption, fabricated source retirement, incorrect wire credits, and forced exits.

The QuickJS job-context unit keeps its original 20-second budget and original context tests. Additional properties cover late observation, async preludes, revoked subscribers, partial capture failure, ordinary mixed inputs, intrinsic forwarding, overridden `then`, and interruption during reaction conversion.

## Upgrade fallback gzip

A fetch request with `Upgrade: websocket` can receive an ordinary HTTP response.
Its body uses the existing credit-controlled native channel and its single
transport owner. A gzip response is decoded before its bytes reach the Fetch
body reader. The encoded HTTP headers remain available on the response.

The decoder uses the same native gzip parser and allocator as ordinary fetch.
It is CPU-only and creates no second HTTP client or connection. It writes only
into the current reader's buffer, in steps of at most 4,096 decoded bytes, and
returns channel credit for encoded input actually consumed. Pending data stays
inside the channel window. Cancellation, parser failure and EOF dispose the
decoder and follow the existing channel cancellation and retirement paths.
The existing 101 WebSocket path and raw response body retain their behavior.

An enabled response body limit counts decoded bytes. An encoded Content-Length
does not reject an otherwise valid decoded body. The C ABI limit fixture sends
real stored DEFLATE blocks with a gzip header, CRC32 and ISIZE. An independent
zlib decoder verifies the complete source vectors and rejects altered CRC and
missing trailers. Full-body fragmented cases wait for the previous fragment's
actual native credit before producing the next fragment. Consumer cancellation
keeps the trailer and EOF withheld. Raw source cases and their budgets remain.

The DefaultReader gates use header readiness followed by 69 consumer read
acknowledgments for 274,449 bytes. The source holds EOF until the final read.
Raw and gzip streaming and cancellation are compared with the same locked
primary program, including full backing hashes and reader closed/release state.
A real whole-body wait cannot acknowledge the first held part and must trigger
the two second source gate. Each positive run also checks source socket closure,
a healthy subsequent request and a natural native process exit. Ordinary fetch
and Upgrade fallback have separate required registrations. Their independent
report contract rejects omitted cases, weaker deadlines, changed programs,
changed public state, early source EOF, leaked sockets and forced cleanup.

These checks establish functional behavior. They do not replace final combined
acceptance, sanitizer findings, resource soak or formal performance qualification.

## Iterator storage

The native VM keeps an array or string iterator's cursor and retained input in
the allocation that owns the iterator object. The JavaScript object layout and
the layouts of other classes stay the same. The object's allocation includes
enough aligned space for the cursor; its existing finalizer releases the input,
and the ordinary object free releases the complete allocation.

An empty iterator initially has no own-property allocation. Adding its first
own property allocates the existing shape's capacity before the shape or value
is changed. Later property growth, deletion, compaction, accessors, symbols and
prototype changes follow the existing object paths. The iterator remains
extensible, and freezing the JavaScript object does not freeze its internal
cursor. Array mutation, typed-array detachment and string code points retain
their normal iterator behavior.

Memory accounting includes the inline cursor in object bytes and counts only
allocations that exist. Garbage collection retains the same input edge and can
collect cycles involving an iterator, its input and application properties.
No iterator object, user getter, return hook, EOF or cancellation transition is
replaced by a shortcut.

`quickjs_iterator_storage` checks observable iterator behavior, 2,048 cyclic
ownership graphs, exact allocator and memory counters, and all 38 allocation
failure points of iterator creation followed by own-property growth. Its CTest
budget is 20 seconds. The original job-context properties, original Hono guest
and 500 millisecond application deadline, native fetch matrices, and retired
branch contracts remain separate gates. Private timing observations are not a
formal performance or density qualification.
