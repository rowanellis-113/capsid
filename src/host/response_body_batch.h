// Bounded HTTP response-body write coalescing.
//
// Worker IPC frames stay independently credit-accounted. Once consecutive
// frames for one request are already buffered on the Host, however, writing
// each 4 KiB frame through Beast separately adds one async operation and one
// sendmsg without improving backpressure. This helper joins only adjacent
// frames with the same credit-return state and never exceeds the caller's
// existing bounded-buffer window.
#ifndef CAPSID_SRC_HOST_RESPONSE_BODY_BATCH_H_
#define CAPSID_SRC_HOST_RESPONSE_BODY_BATCH_H_

#include <cassert>
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

namespace capsid::host {

inline constexpr std::size_t kResponseBodyWriteBatchLimit = 64u * 1024u;

template <typename QueuedBody>
QueuedBody take_coalesced_response_body(std::vector<QueuedBody>* queue, std::size_t* queued_bytes,
                                        std::size_t limit) {
    assert(queue != nullptr);
    assert(queued_bytes != nullptr);
    assert(!queue->empty());

    QueuedBody result = std::move((*queue)[0]);
    assert(*queued_bytes >= result.bytes.size());
    *queued_bytes -= result.bytes.size();

    std::size_t combined_size = result.bytes.size();
    std::size_t combine_count = 0;
    for (std::size_t index = 1; index < queue->size(); ++index) {
        const QueuedBody& next = (*queue)[index];
        if (next.credit_returned_early != result.credit_returned_early || combined_size > limit ||
            next.bytes.size() > limit - combined_size) {
            break;
        }
        combined_size += next.bytes.size();
        ++combine_count;
    }
    if (combine_count > 0) {
        result.bytes.reserve(combined_size);
        for (std::size_t index = 1; index <= combine_count; ++index) {
            QueuedBody& next = (*queue)[index];
            assert(*queued_bytes >= next.bytes.size());
            *queued_bytes -= next.bytes.size();
            result.bytes.insert(result.bytes.end(), std::make_move_iterator(next.bytes.begin()),
                                std::make_move_iterator(next.bytes.end()));
        }
    }
    const std::size_t consumed = 1 + combine_count;
    if (consumed == queue->size()) {
        queue->clear();
    } else {
        queue->erase(queue->begin(), queue->begin() + static_cast<std::ptrdiff_t>(consumed));
    }
    return result;
}

} // namespace capsid::host

#endif // CAPSID_SRC_HOST_RESPONSE_BODY_BATCH_H_
