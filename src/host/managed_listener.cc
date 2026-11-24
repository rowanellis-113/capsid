// ManagedListener implementation — see managed_listener.h. One io thread
// runs a Boost.Beast acceptor and every client session; worker responses
// arrive through the §9.2 event sink (pool pump thread → WorkerEventMailbox
// → io thread). The request/response state machine is keyed per
// (executor, request_id) because different executors have independent
// request-id spaces.

#include "host/managed_listener.h"
#include "host/channel_ledger.h"
#include "host/client_disconnect.h"
#include "host/credit_limits.h"
#include "host/metrics.h"
#include "net_target.h"

#include "host/listener_cors.h"
#include "host/request_normalization.h"
#include "host/response_body_batch.h"
#include "host/structured_log.h"
#include "host/worker_executor.h"
#include "host/websocket_session.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace capsid::host {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;

// Defined at capsid::host scope after the anonymous namespace (the M1A
// server pattern); the event-bridge types below only hold the pointer.
class ListenerSession;

namespace {

void write_stderr(std::string_view message) {
    std::fwrite(message.data(), 1, message.size(), stderr);
    std::fputc('\n', stderr);
}

// RFC 7230 tchar: the only legal header-name characters.
bool is_token_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    switch (c) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
        return true;
    default:
        return false;
    }
}

// RFC 7230 field-value byte: HTAB / SP / VCHAR / obs-text.
bool valid_field_value_byte(unsigned char c) {
    return c == '\t' || (c >= 0x20 && c <= 0x7e) || c >= 0x80;
}

std::string ascii_lower(std::string_view text) {
    std::string lower(text);
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return lower;
}

bool is_event_stream_content_type(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
        ++begin;
    }
    static constexpr std::string_view kEventStream = "text/event-stream";
    if (value.size() - begin < kEventStream.size() ||
        !std::equal(
            kEventStream.begin(), kEventStream.end(),
            value.begin() + static_cast<std::ptrdiff_t>(begin),
            [](unsigned char a, unsigned char b) { return std::tolower(a) == std::tolower(b); })) {
        return false;
    }
    std::size_t end = begin + kEventStream.size();
    while (end < value.size() && (value[end] == ' ' || value[end] == '\t')) {
        ++end;
    }
    return end == value.size() || value[end] == ';';
}

std::size_t estimate_request_bytes(const http::request<http::string_body>& request) {
    std::size_t bytes =
        request.method_string().size() + request.target().size() + request.body().size() + 8;
    for (const auto& field : request.base()) {
        bytes += field.name_string().size() + field.value().size() + 4;
    }
    return bytes;
}

// Parses the comma-separated token list of a Connection header and collects
// the (lowercased) nominated names. Returns false on an empty or non-token
// nomination, which fails the response closed (RFC 7230 §6.1).
bool collect_connection_nominations(std::string_view value, std::set<std::string>* nominated) {
    std::size_t begin = 0;
    while (begin <= value.size()) {
        while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t')) {
            ++begin;
        }
        const std::size_t end = value.find(',', begin);
        const std::size_t token_end = end == std::string_view::npos ? value.size() : end;
        std::size_t trimmed = token_end;
        while (trimmed > begin && (value[trimmed - 1] == ' ' || value[trimmed - 1] == '\t')) {
            --trimmed;
        }
        if (trimmed == begin) {
            return false;
        }
        for (std::size_t index = begin; index < trimmed; ++index) {
            if (!is_token_char(static_cast<unsigned char>(value[index]))) {
                return false;
            }
        }
        nominated->insert(ascii_lower(value.substr(begin, trimmed - begin)));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return true;
}

// The cross-thread bridge for worker events (§9.2). The pool's pump thread
// (the sole drainer of every executor's event queue) posts events here; the
// listener io thread drains them and dispatches into the session layer.
//
// The mailbox OWNS the io_context and lives as long as BOTH the listener
// (its ManagedListenerImpl holds a shared_ptr) and the pool (the event sink and the
// client-bytes loader hold shared_ptrs). The io_context is therefore
// destroyed only when the pump thread is joined and the io thread is
// joined — a post can never race its destruction.
//
// Lock-order rule: post() takes mutex_ and posts onto the io_context while
// still holding it. The pool invokes the sink under the pool mutex, so the
// io thread must NEVER take a lock the pool can hold while holding mutex_
// (it drains with a swap under mutex_ and dispatches AFTER releasing, so
// pick_worker/submit inside a dispatch never nest with mutex_).
class WorkerEventMailbox : public std::enable_shared_from_this<WorkerEventMailbox> {
  public:
    asio::io_context ioc;
    // One-time wiring: the io-side dispatch target. Must be called before
    // the io thread starts — the drain reads it without the mutex, and
    // post() reads it under the mutex, so it never changes afterwards.
    void
    set_dispatcher(std::function<void(std::shared_ptr<WorkerExecutor>, WorkerEvent)> dispatcher) {
        std::lock_guard<std::mutex> lock(mutex_);
        dispatcher_ = std::move(dispatcher);
    }

    void post(std::shared_ptr<WorkerExecutor> executor, WorkerEvent event) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || !dispatcher_) {
            return; // fail closed: no listener side to serve the event
        }
        events_.emplace_back(std::move(executor), std::move(event));
        // Post under mutex_: serialized against close()/destruction, so a
        // post can never race the io_context.
        asio::post(ioc, [weak = weak_from_this()] {
            if (std::shared_ptr<WorkerEventMailbox> mailbox = weak.lock()) {
                mailbox->drain();
            }
        });
    }

    void post_task(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || !dispatcher_ || !task) {
            return;
        }
        asio::post(ioc, [weak = weak_from_this(), task = std::move(task)]() mutable {
            if (std::shared_ptr<WorkerEventMailbox> mailbox = weak.lock()) {
                bool closed = false;
                {
                    std::lock_guard<std::mutex> guard(mailbox->mutex_);
                    closed = mailbox->closed_;
                }
                if (!closed) {
                    task();
                }
            }
        });
    }

    // §8.2 "bytes awaiting client consumption" per executor, for the pool's
    // pick_worker loader. Written by the io thread (write submissions and
    // completions), read by the pump thread under the pool mutex; guarded
    // by mutex_.
    void add_client_bytes(std::uint64_t executor_id, std::size_t bytes) {
        if (bytes == 0 || executor_id == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        client_bytes_[executor_id] += bytes;
    }

    void sub_client_bytes(std::uint64_t executor_id, std::size_t bytes) {
        if (bytes == 0 || executor_id == 0) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = client_bytes_.find(executor_id);
        if (it == client_bytes_.end()) {
            return;
        }
        if (it->second <= bytes) {
            client_bytes_.erase(it);
        } else {
            it->second -= bytes;
        }
    }

    std::uint64_t load_of(const WorkerExecutor* executor) const {
        if (executor == nullptr) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = client_bytes_.find(executor->id());
        return it == client_bytes_.end() ? 0 : it->second;
    }

    void erase_executor(std::uint64_t executor_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        client_bytes_.erase(executor_id);
    }

    // Drops future posts (the listener is stopping). Existing queued events
    // are drained by the io thread and dispatched to an expiring listener —
    // their sessions are being closed anyway.
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
    }

  private:
    // Set once via set_dispatcher() before the io thread starts; read by
    // the drain and by post() (under mutex_).
    std::function<void(std::shared_ptr<WorkerExecutor>, WorkerEvent)> dispatcher_;

    void drain() {
        std::deque<std::pair<std::shared_ptr<WorkerExecutor>, WorkerEvent>> local;
        {
            // Swap + dispatch AFTER release (lock-order rule in the class
            // comment): the dispatcher can route through acquire_worker/pick_worker,
            // which takes the pool mutex.
            std::lock_guard<std::mutex> lock(mutex_);
            local.swap(events_);
        }
        for (auto& [executor, event] : local) {
            if (dispatcher_) {
                dispatcher_(std::move(executor), std::move(event));
            }
        }
    }

    mutable std::mutex mutex_;
    bool closed_ = false;
    std::deque<std::pair<std::shared_ptr<WorkerExecutor>, WorkerEvent>> events_;
    std::map<std::uint64_t, std::uint64_t> client_bytes_;
};

// Early-credit window (host, spec §8.1): response credit is returned at
// receive time while the per-request HTTP write queue is below this;
// beyond it, credit falls back to write-completion, so a slow client
// cannot make the host buffer unboundedly.
static constexpr std::size_t kEarlyCreditWindow = 64u * 1024u;
// Minimum response-credit batch before sending a kGrantResponseCredit command
// mid-stream; smaller tails flush immediately at write completion or end.
static constexpr std::uint32_t kCreditFlushThreshold = 16u * 1024u;

// Request state owned by the io thread only.
struct PendingRequest {
    std::shared_ptr<GenerationPool> pool;
    std::shared_ptr<ChannelLease> lease;
    std::string upgrade_key, upgrade_protocols;
    std::shared_ptr<WorkerExecutor> executor;
    std::shared_ptr<ListenerSession> session;
    std::string method;
    bool keep_alive = false;
    unsigned version = 11;

    // Request direction.
    std::string request_body;
    std::size_t request_body_offset = 0;
    std::uint64_t request_credit = 0;
    bool request_ended = false;

    // Response direction. Shared ownership keeps the serializer alive
    // through in-flight async writes after the request is erased.
    std::shared_ptr<http::response<http::buffer_body>> response;
    std::shared_ptr<http::response_serializer<http::buffer_body>> serializer;
    // Complete responses advertised by the bounded fixed-body protocol are
    // accumulated to their exact <=4 KiB size and emitted in one HTTP write.
    std::shared_ptr<http::response<http::vector_body<std::uint8_t>>> fixed_response;
    std::shared_ptr<http::response_serializer<http::vector_body<std::uint8_t>>> fixed_serializer;
    std::size_t fixed_body_expected = 0;
    std::size_t fixed_body_received = 0;
    bool fixed_write_started = false;
    std::vector<std::uint8_t> outgoing;
    // Response-credit protocol: each queued block remembers whether its
    // credit was returned at receive time (early window) or must be
    // returned on the write completion.
    struct QueuedBody {
        std::vector<std::uint8_t> bytes;
        bool credit_returned_early = false;
    };
    std::deque<QueuedBody> body_queue;
    std::size_t body_queue_bytes = 0; // client-bytes accounting
    std::uint32_t pending_response_credit = 0;
    bool outgoing_credit_returned_early = false;
    bool head_sent = false;
    bool head_only = false;
    bool writing = false;
    bool end_seen = false;
    bool cl_known = false;
    std::size_t cl_remaining = 0;
    bool holds_streaming_permit = false;
    std::optional<asio::steady_timer> idle_timer;
    // E-3 §9.2 write deadline around one socket write.
    std::optional<asio::steady_timer> write_timer;
};

} // namespace

// One client connection. Owns the socket, the parser and the read
// deadline; the request state lives in ManagedListenerImpl::requests_.
class ListenerSession : public std::enable_shared_from_this<ListenerSession> {
    friend class ManagedListenerImpl;

  public:
    ListenerSession(std::shared_ptr<ManagedListenerImpl> impl, tcp::socket socket);

    void start();
    void close();
    bool closed() const { return closed_; }

    beast::tcp_stream& stream() { return stream_; }

    // Sends a small synchronous response and returns to reading (or closes
    // when keep_alive is false).
    void send_simple(http::status status, std::string_view body, bool keep_alive, unsigned version);

  private:
    void read_head();
    void read_body();
    void handle_request(http::request<http::string_body> request);
    // Listener-level CORS: answers a browser preflight (returns false) or
    // records the matched origin for response stamping (returns true).
    bool prepare_cors(const http::request<http::string_body>& request);
    void send_cors_preflight(const http::request<http::string_body>& request);
    void arm_read_timer(bool head_phase);
    void on_read_timeout();
    void start_disconnect_probe();

    std::shared_ptr<ManagedListenerImpl> impl_;
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<http::request_parser<http::string_body>> parser_;
    // The current in-flight request (for the disconnect probe and close).
    std::shared_ptr<WorkerExecutor> current_executor_;
    std::optional<std::uint64_t> current_request_id_;
    asio::steady_timer read_timer_;
    // The shared listener-CORS decision engine, engaged only while the
    // listener owns CORS (config.configured). One request is in flight per
    // session at a time, so a single per-session instance is exact.
    std::optional<ListenerCors> cors_;
    bool closed_ = false;
    bool probe_active_ = false;
    bool last_request_head_ = false;
    std::weak_ptr<WebSocketSession> websocket_;
    std::shared_ptr<GenerationPool> current_pool_;
};

// The request/response state machine and the io thread.
class ManagedListenerImpl : public std::enable_shared_from_this<ManagedListenerImpl> {
    friend class ListenerSession;
    // The facade needs mailbox_ (wire_pool) and stop_requested_ (running).
    friend class ManagedListener;

  public:
    explicit ManagedListenerImpl(ManagedListenerOptions options)
        : options_(std::move(options)), mailbox_(std::make_shared<WorkerEventMailbox>()),
          max_request_body_bytes_(options_.max_request_body_bytes),
          credit_grant_threshold_(
              clamp_credit_grant_threshold(kCreditFlushThreshold, options_.initial_stream_window)) {
        const char* bodyless_env = std::getenv("CAPSID_BODYLESS");
        if (bodyless_env != nullptr && std::strcmp(bodyless_env, "0") == 0) {
            bodyless_enabled_ = false;
            if (options_.log != nullptr) {
                options_.log->log(LogLane::kControl,
                                  {.level = "warn",
                                   .event = log_events::kStartup,
                                   .message = "bodyless request fusion disabled"});
            } else {
                write_stderr("capsid-host: bodyless request fusion disabled");
            }
        }
        if (options_.metrics) {
            options_.metrics->observe_channel_budget(service_channel_budget_);
            options_.metrics->observe_channel_budget(
                channel_budget_, options_.config.name.empty() ? "local" : options_.config.name);
        }
    }

    ~ManagedListenerImpl() {
        request_stop();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    }

    bool start(std::string* error);
    void stop_accepting();
    void request_stop();
    void request_stop(std::function<void()> on_io_stopped);
    bool wait(std::string* error);
    std::uint16_t bound_port() const { return bound_port_; }

  private:
    // ---- io-thread helpers ----------------------------------------------

    void io_loop();
    void do_accept();
    void stop_from_io();
    void drop_session(const std::shared_ptr<ListenerSession>& session);
    void dispatch_request(const std::shared_ptr<GenerationPool>& pool,
                          GenerationPool::DispatchLease lease,
                          const std::shared_ptr<ListenerSession>& session,
                          const http::request<http::string_body>& request,
                          const NormalizedPublicRequest& normalized);
    void wire_pool(const std::shared_ptr<GenerationPool>& pool);
    void enqueue_or_reject(const std::shared_ptr<GenerationPool>& pool,
                           const std::shared_ptr<ListenerSession>& session,
                           http::request<http::string_body> request,
                           const NormalizedPublicRequest& normalized);
    void pump_pool_queue(const GenerationPool* pool_key);
    void fail_pool_queue_unavailable(const GenerationPool* pool_key);
    struct PoolQueue;
    void expire_front_queued(PoolQueue& pq);
    void arm_queue_timer(const GenerationPool* pool_key);
    void on_queue_timer(const GenerationPool* pool_key, const beast::error_code ec);
    void cancel_queued_for_session(const GenerationPool* pool_key,
                                   const std::shared_ptr<ListenerSession>& session);
    void begin_request(const std::shared_ptr<GenerationPool>& pool,
                       GenerationPool::DispatchLease lease, std::uint64_t request_id,
                       const std::shared_ptr<ListenerSession>& session,
                       const http::request<http::string_body>& request,
                       const NormalizedPublicRequest& normalized);
    void handle_worker_event(std::shared_ptr<WorkerExecutor> executor, WorkerEvent event);
    void advance_request_body(const std::shared_ptr<WorkerExecutor>& executor,
                              std::uint64_t request_id);
    void write_body_block(const std::shared_ptr<WorkerExecutor>& executor, std::uint64_t request_id,
                          std::vector<std::uint8_t> bytes, bool credit_returned_early);
    void flush_pending_credit(const std::shared_ptr<WorkerExecutor>& executor,
                              std::uint64_t request_id, PendingRequest& pending, bool force);
    void write_fixed_response(const std::shared_ptr<WorkerExecutor>& executor,
                              std::uint64_t request_id);
    void write_end_block(const std::shared_ptr<WorkerExecutor>& executor, std::uint64_t request_id);
    void finalize_request(const std::shared_ptr<WorkerExecutor>& executor, std::uint64_t request_id,
                          std::shared_ptr<ListenerSession> session);
    void write_channel_metrics_line();
    bool response_may_complete_early(std::uint64_t executor_id, std::uint64_t request_id) const;
    void cancel_request(const std::shared_ptr<WorkerExecutor>& executor, std::uint64_t request_id);
    void fail_request(const std::shared_ptr<WorkerExecutor>& executor, std::uint64_t request_id,
                      std::shared_ptr<ListenerSession> session);
    void reject_response_head(const std::shared_ptr<WorkerExecutor>& executor,
                              std::uint64_t request_id, const std::string& reason);
    bool sanitize_response_headers(std::vector<std::pair<std::string, std::string>>* headers);
    bool acquire_streaming_permit(std::uint64_t executor_id, PendingRequest& pending);
    void release_streaming_permit(PendingRequest& pending);
    std::uint64_t resolve_stream_idle_timeout_ms(const PendingRequest& pending) const;
    std::uint64_t resolve_write_timeout_ms(const PendingRequest& pending) const;
    void arm_stream_idle_timer(const std::shared_ptr<WorkerExecutor>& executor,
                               std::uint64_t request_id, PendingRequest& pending);
    void on_stream_idle_timer(const std::shared_ptr<WorkerExecutor>& executor,
                              std::uint64_t request_id, const beast::error_code ec);
    void arm_write_timer(std::uint64_t request_id, PendingRequest& pending);
    void disarm_write_timer(PendingRequest& pending);

    struct QueuedRequest {
        std::shared_ptr<ListenerSession> session;
        std::shared_ptr<GenerationPool> pool;
        http::request<http::string_body> request;
        NormalizedPublicRequest normalized;
        std::size_t bytes = 0;
        std::optional<std::chrono::steady_clock::time_point> deadline;
    };

    struct PoolQueue {
        std::deque<QueuedRequest> entries;
        std::size_t queued_bytes = 0;
        std::uint64_t queue_timeout_ms = 0;
        std::optional<asio::steady_timer> timer;
    };

    ManagedListenerOptions options_;
    std::string routing_header_storage_ = "capsid-app";
    std::string resolved_public_authority_ = options_.config.public_authority;
    RequestRoutingPolicy policy_;
    std::shared_ptr<WorkerEventMailbox> mailbox_;
    const std::uint64_t max_request_body_bytes_;
    const std::uint32_t credit_grant_threshold_;
    std::optional<tcp::acceptor> acceptor_;
    std::map<std::uint64_t, std::shared_ptr<ListenerSession>> sessions_;
    std::uint64_t next_session_id_ = 1;
    std::shared_ptr<ServiceChannelBudget> service_channel_budget_ =
        options_.channel_service_budget ? options_.channel_service_budget
                                        : default_channel_budget();
    std::shared_ptr<ServiceChannelBudget> channel_budget_ = std::make_shared<ServiceChannelBudget>(
        ChannelBudgetLimits{options_.config.limits.channels, 0,
                            options_.config.limits.channel_queue_bytes},
        service_channel_budget_);
    ChannelLedger channel_ledger_{ChannelLedger::Limits{}, channel_budget_.get()};
    std::map<std::pair<std::uint64_t, std::uint64_t>, PendingRequest> requests_;
    std::map<std::uint64_t, std::uint64_t> next_request_id_;
    std::map<std::uint64_t, std::uint64_t> streaming_inflight_;
    std::map<const GenerationPool*, PoolQueue> pool_queues_;
    std::map<std::pair<std::uint64_t, std::uint32_t>, std::shared_ptr<WebSocketSession>>
        websockets_;
    std::uint64_t connections_ = 0;
    std::uint16_t bound_port_ = 0;
    std::atomic<bool> stop_requested_ = false;
    std::atomic<bool> started_ = false;
    std::atomic<bool> io_running_ = false;
    bool bodyless_enabled_ = true;
    std::mutex stop_callback_mutex_;
    bool stop_posted_ = false;
    bool io_stopped_ = false;
    std::vector<std::function<void()>> stop_callbacks_;
    std::thread io_thread_;
    std::once_flag wait_once_;
};

ListenerSession::ListenerSession(std::shared_ptr<ManagedListenerImpl> impl, tcp::socket socket)
    : impl_(std::move(impl)), stream_(std::move(socket)), read_timer_(impl_->mailbox_->ioc) {
    // TCP_NODELAY on by default (single-connection latency; benchmark
    // parity with the M1A server).
    const char* nodelay = std::getenv("CAPSID_TCP_NODELAY");
    if (nodelay != nullptr && std::strcmp(nodelay, "0") == 0) {
        write_stderr("capsid-host: TCP_NODELAY disabled");
    } else {
        beast::error_code ec;
        stream_.socket().set_option(asio::ip::tcp::no_delay(true), ec);
    }
}

void ListenerSession::start() { read_head(); }

void ListenerSession::read_head() {
    last_request_head_ = false;
    current_pool_.reset();
    current_executor_.reset();
    current_request_id_.reset();
    parser_ = std::make_shared<http::request_parser<http::string_body>>();
    parser_->body_limit(boost::none);
    const std::uint64_t header_bytes = impl_->options_.config.limits.header_bytes;
    if (header_bytes != 0) {
        parser_->header_limit(static_cast<std::uint32_t>(
            std::min<std::uint64_t>(header_bytes, std::numeric_limits<std::uint32_t>::max())));
    }
    arm_read_timer(true);
    http::async_read_header(
        stream_, buffer_, *parser_,
        [self = shared_from_this(), parser = parser_](beast::error_code ec, std::size_t) {
            self->read_timer_.cancel();
            if (ec) {
                if (ec == http::error::header_limit) {
                    self->send_simple(http::status::request_header_fields_too_large,
                                      "request header limit exceeded", false, 11);
                } else if (ec != asio::error::operation_aborted) {
                    self->close();
                }
                return;
            }
            std::uint64_t limit = self->impl_->max_request_body_bytes_;
            if (self->impl_->options_.routing) {
                const auto& req = parser->get();
                std::vector<PublicRequestHeaderView> views;
                views.reserve(16);
                for (const auto& field : req.base()) {
                    views.push_back({field.name_string(), field.value()});
                }
                const RequestNormalizationResult normalized =
                    normalize_public_request(self->impl_->policy_, req.target(), views);
                if (normalized.ok) {
                    const std::shared_ptr<const RoutingSnapshot> snapshot =
                        self->impl_->options_.routing->load();
                    if (snapshot != nullptr) {
                        if (const std::shared_ptr<GenerationPool> pool =
                                snapshot->find(normalized.request.application)) {
                            limit = pool->max_request_body_bytes();
                        }
                    }
                }
            }
            if (parser->content_length().has_value() && *parser->content_length() > limit) {
                self->send_simple(http::status::payload_too_large, "request body limit exceeded",
                                  false, parser->get().version());
                return;
            }
            parser->body_limit(limit);
            self->read_body();
        });
}

void ListenerSession::read_body() {
    arm_read_timer(false);
    http::async_read(
        stream_, buffer_, *parser_,
        [self = shared_from_this(), parser = parser_](beast::error_code ec, std::size_t) {
            self->read_timer_.cancel();
            if (ec) {
                if (ec == http::error::body_limit) {
                    self->send_simple(http::status::payload_too_large,
                                      "request body limit exceeded", false, 11);
                } else if (ec != asio::error::operation_aborted) {
                    self->close();
                }
                return;
            }
            self->handle_request(parser->release());
        });
}

void ListenerSession::handle_request(http::request<http::string_body> request) {
    last_request_head_ = request.method() == http::verb::head;
    // Listener-level CORS runs before routing: a browser preflight can
    // never carry header-routing's control header (the custom header is
    // exactly what the preflight asks about), so the listener answers it
    // itself; every other request records whether its Origin is allowed
    // so both response paths stamp the matching ACAO.
    if (impl_->options_.config.cors.configured && !prepare_cors(request)) {
        return; // preflight answered (allowed or rejected)
    }
    RequestRoutingPolicy policy = impl_->policy_;
    std::vector<PublicRequestHeaderView> views;
    views.reserve(16);
    for (const auto& field : request.base()) {
        views.push_back({field.name_string(), field.value()});
    }
    const auto rejection = websocket_upgrade_rejection(
        check_websocket_upgrade(views, request.method_string(), request.version()));
    if (rejection.status != 0) {
        send_simple(static_cast<http::status>(rejection.status), rejection.body, false,
                    request.version());
        return;
    }
    if (check_websocket_upgrade(views, request.method_string(), request.version()) ==
            WebSocketUpgradeCheck::kValid &&
        !request.body().empty()) {
        send_simple(http::status::bad_request, "WebSocket upgrade must be bodyless", false,
                    request.version());
        return;
    }
    const RequestNormalizationResult normalized =
        normalize_public_request(policy, request.target(), views);
    if (!normalized.ok) {
        const http::status status =
            normalized.error.code == RequestNormalizationErrorCode::kRouteNotFound
                ? http::status::not_found
                : http::status::bad_request;
        send_simple(status, normalized.error.message, request.keep_alive(), request.version());
        return;
    }
    // One atomic snapshot load; the pool found in it is pinned for the
    // whole request (§9.2). A retired App keeps its route as a tombstone
    // (routed name, no pool): it is permanently gone, so the router
    // answers 404 (§9.6-6), distinct from the 503 reserved for Apps that
    // were never routed.
    const std::shared_ptr<const RoutingSnapshot> snapshot = impl_->options_.routing->load();
    const bool retired = snapshot != nullptr && snapshot->retired(normalized.request.application);
    std::shared_ptr<GenerationPool> pool =
        snapshot ? snapshot->find(normalized.request.application) : nullptr;
    if (retired) {
        send_simple(http::status::not_found, "app retired", request.keep_alive(),
                    request.version());
        return;
    }
    if (!impl_->options_.expected_application.empty() &&
        normalized.request.application != impl_->options_.expected_application) {
        send_simple(http::status::not_found, "app not found", request.keep_alive(),
                    request.version());
        return;
    }
    if (!pool) {
        send_simple(http::status::service_unavailable, "app not found", request.keep_alive(),
                    request.version());
        return;
    }
    bool at_max_inflight = false;
    GenerationPool::DispatchLease lease = pool->acquire_worker(&at_max_inflight);
    if (!lease && !at_max_inflight) {
        if (pool->state() == GenerationPool::State::kActive && pool->is_elastic()) {
            const auto scale_result = pool->request_scale_out();
            lease = pool->acquire_worker(&at_max_inflight);
            if (!lease && !at_max_inflight &&
                (scale_result == GenerationPool::ScaleOutResult::kSpawnInFlight ||
                 pool->scale_out_in_flight())) {
                current_pool_ = pool;
                impl_->enqueue_or_reject(pool, shared_from_this(), std::move(request),
                                         normalized.request);
                impl_->pump_pool_queue(pool.get());
                return;
            }
        }
        if (!lease && !at_max_inflight) {
            send_simple(http::status::service_unavailable, "worker unavailable", false,
                        request.version());
            return;
        }
    }
    const auto q_it = impl_->pool_queues_.find(pool.get());
    const bool queue_empty = q_it == impl_->pool_queues_.end() || q_it->second.entries.empty();
    // The generation's per-worker inflight ceiling (0 = no cap): acquire_worker
    // reports at_max_inflight consistently against inflight() + pending_dispatches().
    if (!queue_empty || at_max_inflight) {
        lease.release();
        if (at_max_inflight && pool->is_elastic()) {
            (void)pool->request_scale_out();
        }
        current_pool_ = pool;
        impl_->enqueue_or_reject(pool, shared_from_this(), std::move(request), normalized.request);
        return;
    }
    impl_->dispatch_request(pool, std::move(lease), shared_from_this(), request,
                            normalized.request);
}

bool ListenerSession::prepare_cors(const http::request<http::string_body>& request) {
    cors_.emplace(impl_->options_.config.cors);
    const CorsDecision decision = cors_->prepare(request);
    if (decision == CorsDecision::kBadRequest) {
        send_simple(http::status::bad_request, "duplicate Origin header", request.keep_alive(),
                    request.version());
        return false;
    }
    if (decision == CorsDecision::kPreflightAllowed ||
        decision == CorsDecision::kPreflightRejected) {
        send_cors_preflight(request);
        return false;
    }
    return true;
}

void ListenerSession::send_cors_preflight(const http::request<http::string_body>& request) {
    auto response = std::make_shared<http::response<http::string_body>>();
    response->version(request.version());
    response->keep_alive(request.keep_alive());
    cors_->build_preflight(*response);
    response->prepare_payload();
    http::async_write(
        stream_, *response,
        [self = shared_from_this(), response, request](beast::error_code ec, std::size_t) {
            (void)response;
            (void)request;
            if (!ec && request.keep_alive() && !self->closed_) {
                self->read_head();
            } else {
                self->close();
            }
        });
}

void ListenerSession::arm_read_timer(bool head_phase) {
    const std::uint64_t ms = head_phase ? impl_->options_.config.limits.header_timeout_ms
                                        : impl_->options_.config.limits.body_idle_timeout_ms;
    if (ms == 0) {
        return;
    }
    read_timer_.expires_after(std::chrono::milliseconds(ms));
    read_timer_.async_wait([self = shared_from_this()](const beast::error_code ec) {
        if (ec) {
            return; // cancelled — the read finished first
        }
        self->on_read_timeout();
    });
}

// v1 read deadline: close the connection (408 synthesis is deferred to the
// follow-up listener PR). close() cancels the pending read; its completion
// handler sees operation_aborted and returns.
void ListenerSession::on_read_timeout() { close(); }

// While a request is in flight the Host keeps an async_wait on the socket
// so an abortive peer disconnect (RST) cancels the worker request
// immediately (§9.3) instead of waiting for the worker deadline. Mirrors
// the M1A probe.
void ListenerSession::start_disconnect_probe() {
    if (current_executor_ && current_request_id_) {
        const auto found = impl_->requests_.find({current_executor_->id(), *current_request_id_});
        if (found != impl_->requests_.end() && !found->second.upgrade_key.empty())
            return;
    }
    if (probe_active_ || closed_) {
        return;
    }
    probe_active_ = true;
    const auto request_id = current_request_id_;
    const auto executor = current_executor_;
    stream_.socket().async_wait(tcp::socket::wait_read, [self = shared_from_this(), request_id,
                                                         executor](beast::error_code ec) {
        self->probe_active_ = false;
        if (self->closed_) {
            return;
        }
        // The socket fired while no request was in flight (the previous
        // request finished and its bytes belong to the next parse):
        // leave the data in place and re-arm from the next
        // handle_request.
        if (self->current_request_id_ != request_id || self->current_executor_ != executor ||
            !self->current_request_id_) {
            self->start_disconnect_probe();
            return;
        }
        if (ec) {
            self->close();
            return;
        }
        const auto probe = probe_client_read(self->stream_.socket());
        if (probe == ClientReadProbe::kRetry) {
            self->start_disconnect_probe();
        } else if (probe == ClientReadProbe::kError) {
            self->close();
        } else if (probe == ClientReadProbe::kData &&
                   !self->impl_->response_may_complete_early(executor->id(), *request_id)) {
            // Once the last response block is submitted a keep-alive
            // client may send its next request before our write callback.
            // Earlier pipelined input remains forbidden.
            self->close();
        }
    });
}

void ListenerSession::close() {
    if (closed_) {
        return;
    }
    closed_ = true;
    if (auto websocket = websocket_.lock())
        websocket->abort();
    read_timer_.cancel();
    if (current_request_id_ && current_executor_) {
        const auto executor = std::move(current_executor_);
        const std::uint64_t req_id = *current_request_id_;
        current_executor_.reset();
        current_request_id_.reset();
        impl_->cancel_request(executor, req_id);
    } else {
        current_executor_.reset();
        current_request_id_.reset();
        if (current_pool_) {
            impl_->cancel_queued_for_session(current_pool_.get(), shared_from_this());
        }
    }
    current_pool_.reset();
    beast::error_code ec;
    stream_.socket().shutdown(tcp::socket::shutdown_both, ec);
    stream_.close();
    impl_->drop_session(shared_from_this());
}

void ListenerSession::send_simple(http::status status, std::string_view body, bool keep_alive,
                                  unsigned version) {
    auto response = std::make_shared<http::response<http::string_body>>();
    response->result(status);
    response->version(version);
    response->keep_alive(keep_alive);
    if (status == http::status::upgrade_required) {
        response->set(http::field::sec_websocket_version, "13");
    }
    response->set(http::field::content_type, "text/plain");
    if (cors_) {
        cors_->stamp(*response);
    }
    if (last_request_head_) {
        response->content_length(body.size());
    } else {
        response->body() = std::string(body);
        response->prepare_payload();
    }
    http::async_write(
        stream_, *response,
        [self = shared_from_this(), response, keep_alive](beast::error_code ec, std::size_t) {
            (void)response;
            if (!ec && keep_alive && !self->closed_) {
                self->read_head();
            } else {
                self->close();
            }
        });
}

// --------------------------------------------------------------------------
// ManagedListenerImpl
// --------------------------------------------------------------------------

bool ManagedListenerImpl::start(std::string* error) {
    if (started_.exchange(true)) {
        if (error != nullptr) {
            *error = "listener already started";
        }
        return false;
    }
    if (stop_requested_.load(std::memory_order_acquire)) {
        if (error != nullptr) {
            *error = "listener stop was requested before start";
        }
        return false;
    }
    if (!options_.routing) {
        if (error != nullptr) {
            *error = "listener requires a routing table";
        }
        return false;
    }

    // Compile + validate the routing policy BEFORE binding: a header-mode
    // listener that is not trusted fails closed at startup (§3.7/§8.1), and
    // a malformed authority never serves a single request.
    const std::string& mode = options_.config.routing.mode;
    if (mode == "path") {
        policy_.mode = RequestRoutingMode::kPath;
    } else if (mode == "subdomain") {
        policy_.mode = RequestRoutingMode::kSubdomain;
    } else if (mode == "header") {
        policy_.mode = RequestRoutingMode::kHeader;
        if (!options_.config.routing.header.empty()) {
            routing_header_storage_ = ascii_lower(options_.config.routing.header);
        } else if (!options_.config.routing.suffix.empty()) {
            routing_header_storage_ = ascii_lower(options_.config.routing.suffix);
        } else {
            routing_header_storage_ = "capsid-app";
        }
        policy_.routing_header = routing_header_storage_;
    } else if (mode == "single") {
        policy_.mode = RequestRoutingMode::kSingle;
        policy_.single_application = options_.config.routing.single_app;
    } else {
        if (error != nullptr) {
            *error = "listener routing mode must be path, subdomain, header or single";
        }
        return false;
    }
    policy_.public_scheme = (mode == "single" && options_.config.public_scheme.empty())
                                ? std::string_view("http")
                                : std::string_view(options_.config.public_scheme);
    policy_.subdomain_suffix = options_.config.routing.suffix;
    resolved_public_authority_ = options_.config.public_authority;
    const bool deferred_zero_port =
        options_.config.public_origin_auto && resolved_public_authority_.size() >= 2 &&
        resolved_public_authority_.compare(resolved_public_authority_.size() - 2, 2, ":0") == 0;
    std::string prebind_authority_storage;
    if (deferred_zero_port) {
        prebind_authority_storage =
            resolved_public_authority_.substr(0, resolved_public_authority_.size() - 1) + "1";
        policy_.public_authority = prebind_authority_storage;
    } else {
        policy_.public_authority = resolved_public_authority_;
    }
    policy_.trusted_header_routing = options_.config.trusted;
    RequestNormalizationError policy_error;
    if (!is_valid_routing_policy(policy_, &policy_error)) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name +
                     ": invalid routing policy: " + policy_error.message;
        }
        return false;
    }

    // config.tcp accepts "host", "host:port", "[ipv6]", or "[ipv6]:port".
    const std::string& tcp_text = options_.config.tcp;
    std::string host;
    std::uint16_t port = 80;
    if (!tcp_text.empty() && tcp_text.front() == '[' && tcp_text.back() == ']') {
        host = tcp_text.substr(1, tcp_text.size() - 2);
    } else if (tcp_text.find(':') == std::string::npos) {
        host = tcp_text;
    } else if (!capsid::parse_tcp_endpoint(tcp_text, &host, &port, /*allow_zero_port=*/true)) {
        if (error != nullptr) {
            *error =
                "listener " + options_.config.name + ": invalid tcp endpoint '" + tcp_text + "'";
        }
        return false;
    }
    if (host.empty()) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": empty tcp host";
        }
        return false;
    }

    beast::error_code ec;
    const tcp::endpoint endpoint(asio::ip::make_address(host, ec), port);
    if (ec) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": invalid tcp address '" + host + "'";
        }
        return false;
    }
    acceptor_.emplace(mailbox_->ioc);
    acceptor_->open(endpoint.protocol(), ec);
    if (ec) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": open failed: " + ec.message();
        }
        return false;
    }
    acceptor_->set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": option failed: " + ec.message();
        }
        return false;
    }
    acceptor_->bind(endpoint, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": bind failed: " + ec.message();
        }
        return false;
    }
    acceptor_->listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        if (error != nullptr) {
            *error = "listener " + options_.config.name + ": listen failed: " + ec.message();
        }
        return false;
    }
    bound_port_ = acceptor_->local_endpoint(ec).port();
    if (ec) {
        if (error != nullptr) {
            *error =
                "listener " + options_.config.name + ": local endpoint failed: " + ec.message();
        }
        return false;
    }
    if (deferred_zero_port && bound_port_ > 0) {
        resolved_public_authority_ =
            resolved_public_authority_.substr(0, resolved_public_authority_.size() - 1) +
            std::to_string(bound_port_);
        policy_.public_authority = resolved_public_authority_;
        RequestNormalizationError bound_policy_error;
        if (!is_valid_routing_policy(policy_, &bound_policy_error)) {
            beast::error_code close_ec;
            acceptor_->close(close_ec);
            acceptor_.reset();
            if (error != nullptr) {
                *error = "listener " + options_.config.name +
                         ": invalid routing policy: " + bound_policy_error.message;
            }
            return false;
        }
    }

    // Wire the event bridge (§9.2): every pool in the current snapshot
    // forwards its events into our mailbox, whose dispatcher routes them
    // into this io thread. Both wirings precede the io thread (and thus
    // the first accept), so a request can never pin a worker before its
    // response path exists. The sink swap is pool-mutex-guarded and the
    // dispatcher is set under the mailbox mutex, so neither wiring can be
    // observed half-made.
    if (const std::shared_ptr<const RoutingSnapshot> snapshot = options_.routing->load()) {
        for (const auto& [application, pool] : snapshot->routes()) {
            (void)application;
            wire_pool(pool);
        }
    }
    mailbox_->set_dispatcher(
        [weak = weak_from_this()](std::shared_ptr<WorkerExecutor> executor, WorkerEvent event) {
            if (std::shared_ptr<ManagedListenerImpl> self = weak.lock()) {
                self->handle_worker_event(std::move(executor), std::move(event));
            }
        });

    io_running_.store(true, std::memory_order_release);
    io_thread_ = std::thread([self = shared_from_this()] { self->io_loop(); });
    return true;
}

void ManagedListenerImpl::io_loop() {
    do_accept();
    mailbox_->ioc.run();
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(stop_callback_mutex_);
        io_stopped_ = true;
        io_running_.store(false, std::memory_order_release);
        callbacks.swap(stop_callbacks_);
    }
    for (auto& cb : callbacks) {
        if (cb) {
            cb();
        }
    }
}

void ManagedListenerImpl::do_accept() {
    if (!acceptor_) {
        return;
    }
    acceptor_->async_accept([self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                self->do_accept();
            }
            return;
        }
        // §9.2 connection ceiling: an excess connection is closed
        // without service.
        const std::uint64_t ceiling = self->options_.config.limits.connections;
        if (ceiling != 0 && self->connections_ >= ceiling) {
            beast::error_code ignored;
            socket.close(ignored);
            self->do_accept();
            return;
        }
        // Erasing a non-tail session leaves holes: size()+1 can
        // overwrite a live connection and lose its shutdown owner.
        if (self->next_session_id_ == 0) {
            beast::error_code ignored;
            socket.close(ignored);
            self->do_accept();
            return;
        }
        const std::uint64_t session_id = self->next_session_id_++;
        ++self->connections_;
        std::shared_ptr<ListenerSession> session =
            std::make_shared<ListenerSession>(self, std::move(socket));
        self->sessions_.emplace(session_id, session);
        session->start();
        self->do_accept();
    });
}

void ManagedListenerImpl::stop_accepting() {
    asio::post(mailbox_->ioc, [weak = weak_from_this()] {
        if (std::shared_ptr<ManagedListenerImpl> self = weak.lock()) {
            self->acceptor_.reset();
        }
    });
}

void ManagedListenerImpl::request_stop() { request_stop(std::function<void()>()); }

void ManagedListenerImpl::request_stop(std::function<void()> on_io_stopped) {
    bool invoke_now = false;
    bool should_post = false;
    {
        std::lock_guard<std::mutex> lock(stop_callback_mutex_);
        if (io_stopped_ || !io_running_.load(std::memory_order_acquire)) {
            invoke_now = static_cast<bool>(on_io_stopped);
        } else {
            if (on_io_stopped) {
                stop_callbacks_.push_back(std::move(on_io_stopped));
            }
            should_post = !stop_posted_;
            stop_posted_ = true;
        }
    }
    stop_requested_.store(true, std::memory_order_release);
    // Fail closed at the mailbox: late events drop instead of accumulating.
    mailbox_->close();
    if (invoke_now) {
        on_io_stopped();
        return;
    }
    if (should_post) {
        // Wake the io thread; it closes the acceptor + every session, and run()
        // drains naturally. The post goes through the mailbox, which outlives
        // this ManagedListenerImpl (the pool's sink holds it), so the wake is always safe.
        asio::post(mailbox_->ioc, [weak = weak_from_this()] {
            if (std::shared_ptr<ManagedListenerImpl> self = weak.lock()) {
                self->stop_from_io();
            }
        });
    }
}

void ManagedListenerImpl::stop_from_io() {
    acceptor_.reset(); // cancels the pending async_accept
    for (auto& [pool_key, pq] : pool_queues_) {
        (void)pool_key;
        if (pq.timer) {
            pq.timer->cancel();
            pq.timer.reset();
        }
        pq.entries.clear();
        pq.queued_bytes = 0;
    }
    pool_queues_.clear();
    const std::vector<std::uint64_t> session_ids = [this] {
        std::vector<std::uint64_t> ids;
        ids.reserve(sessions_.size());
        for (const auto& entry : sessions_) {
            ids.push_back(entry.first);
        }
        return ids;
    }();
    std::vector<std::shared_ptr<WebSocketSession>> active_websockets;
    active_websockets.reserve(websockets_.size());
    for (const auto& entry : websockets_) {
        if (entry.second) {
            active_websockets.push_back(entry.second);
        }
    }
    for (const std::uint64_t id : session_ids) {
        auto it = sessions_.find(id);
        if (it != sessions_.end() && it->second->websocket_.expired()) {
            it->second->close();
        }
    }
    for (const auto& ws : active_websockets) {
        ws->shutdown();
    }
    std::vector<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(stop_callback_mutex_);
        io_stopped_ = true;
        callbacks.swap(stop_callbacks_);
    }
    for (auto& cb : callbacks) {
        if (cb) {
            cb();
        }
    }
}

bool ManagedListenerImpl::wait(std::string* error) {
    request_stop(); // wait() implies the stop request (idempotent)
    std::call_once(wait_once_, [this] {
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
    });
    if (error != nullptr) {
        *error = "listener stopped";
    }
    return true;
}

void ManagedListenerImpl::drop_session(const std::shared_ptr<ListenerSession>& session) {
    for (auto it = sessions_.begin(); it != sessions_.end(); ++it) {
        if (it->second == session) {
            sessions_.erase(it);
            if (connections_ > 0) {
                --connections_;
            }
            return;
        }
    }
}

void ManagedListenerImpl::dispatch_request(const std::shared_ptr<GenerationPool>& pool,
                                           GenerationPool::DispatchLease lease,
                                           const std::shared_ptr<ListenerSession>& session,
                                           const http::request<http::string_body>& request,
                                           const NormalizedPublicRequest& normalized) {
    const std::shared_ptr<WorkerExecutor> executor = lease.executor();
    std::uint64_t& next = next_request_id_[executor->id()];
    const std::uint64_t request_id = ++next;
    session->current_executor_ = executor;
    session->current_request_id_ = request_id;
    session->current_pool_ = pool;
    begin_request(pool, std::move(lease), request_id, session, request, normalized);
    session->start_disconnect_probe();
}

void ManagedListenerImpl::enqueue_or_reject(const std::shared_ptr<GenerationPool>& pool,
                                            const std::shared_ptr<ListenerSession>& session,
                                            http::request<http::string_body> request,
                                            const NormalizedPublicRequest& normalized) {
    const std::uint64_t max_q = pool->max_queued_requests();
    if (max_q == 0) {
        session->current_pool_.reset();
        if (options_.metrics != nullptr) {
            options_.metrics->count_pool_rejection(std::string(pool->application_id()),
                                                   std::string(pool->tenant_id()),
                                                   "queue_disabled");
        }
        const bool cold = pool->ready_workers() == 0;
        session->send_simple(cold ? http::status::service_unavailable
                                  : http::status::too_many_requests,
                             cold ? "worker unavailable" : "worker at capacity",
                             cold ? false : request.keep_alive(), request.version());
        return;
    }
    const std::size_t req_bytes = estimate_request_bytes(request);
    const std::uint64_t max_hdr_bytes = pool->queue_header_bytes();
    PoolQueue& pq = pool_queues_[pool.get()];
    if (pq.entries.size() >= max_q ||
        (max_hdr_bytes != 0 && pq.queued_bytes + req_bytes > max_hdr_bytes)) {
        session->current_pool_.reset();
        if (options_.metrics != nullptr) {
            options_.metrics->count_pool_rejection(std::string(pool->application_id()),
                                                   std::string(pool->tenant_id()), "queue_full");
        }
        if (pq.entries.empty()) {
            pool_queues_.erase(pool.get());
        }
        session->send_simple(http::status::too_many_requests, "request queue full",
                             request.keep_alive(), request.version());
        return;
    }
    const std::uint64_t q_timeout_ms = pool->queue_timeout_ms();
    QueuedRequest entry;
    entry.session = session;
    entry.pool = pool;
    entry.request = std::move(request);
    entry.normalized = normalized;
    entry.bytes = req_bytes;
    if (q_timeout_ms != 0) {
        entry.deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(q_timeout_ms);
    }
    pq.queue_timeout_ms = q_timeout_ms;
    pq.entries.push_back(std::move(entry));
    pq.queued_bytes += req_bytes;
    arm_queue_timer(pool.get());
}

void ManagedListenerImpl::pump_pool_queue(const GenerationPool* pool_key) {
    if (pool_key == nullptr) {
        return;
    }
    const auto q_it = pool_queues_.find(pool_key);
    if (q_it == pool_queues_.end()) {
        return;
    }
    PoolQueue& pq = q_it->second;
    const auto now = std::chrono::steady_clock::now();
    while (!pq.entries.empty()) {
        QueuedRequest& front = pq.entries.front();
        if (front.deadline && now >= *front.deadline) {
            expire_front_queued(pq);
            continue;
        }
        std::shared_ptr<GenerationPool> pool = front.pool;
        bool at_max_inflight = false;
        GenerationPool::DispatchLease lease =
            pool ? pool->acquire_worker(&at_max_inflight) : GenerationPool::DispatchLease{};
        if (!lease && !at_max_inflight) {
            if (pool && pool->state() == GenerationPool::State::kActive &&
                (pool->scale_out_in_flight() ||
                 (pool->is_elastic() &&
                  pool->request_scale_out() == GenerationPool::ScaleOutResult::kSpawnInFlight))) {
                break;
            }
            QueuedRequest unavailable = std::move(front);
            pq.entries.pop_front();
            pq.queued_bytes -= unavailable.bytes;
            if (unavailable.session && !unavailable.session->closed()) {
                unavailable.session->current_pool_.reset();
                unavailable.session->send_simple(http::status::service_unavailable,
                                                 "worker unavailable", false,
                                                 unavailable.request.version());
            }
            continue;
        }
        if (!lease || at_max_inflight) {
            lease.release();
            if (pool && pool->is_elastic() && !pool->scale_out_in_flight() &&
                pool->ready_workers() < pool->max_workers()) {
                (void)pool->request_scale_out();
            }
            break;
        }
        QueuedRequest entry = std::move(front);
        pq.entries.pop_front();
        pq.queued_bytes -= entry.bytes;
        if (!entry.session || entry.session->closed()) {
            continue;
        }
        dispatch_request(entry.pool, std::move(lease), entry.session, entry.request,
                         entry.normalized);
    }
    arm_queue_timer(pool_key);
}

void ManagedListenerImpl::fail_pool_queue_unavailable(const GenerationPool* pool_key) {
    if (pool_key == nullptr) {
        return;
    }
    const auto q_it = pool_queues_.find(pool_key);
    if (q_it == pool_queues_.end()) {
        return;
    }
    PoolQueue& pq = q_it->second;
    while (!pq.entries.empty()) {
        std::shared_ptr<GenerationPool> pool = pq.entries.front().pool;
        if (pool && (pool->pick_worker() != nullptr || pool->scale_out_in_flight())) {
            pump_pool_queue(pool_key);
            return;
        }
        QueuedRequest unavailable = std::move(pq.entries.front());
        pq.entries.pop_front();
        pq.queued_bytes -= unavailable.bytes;
        if (unavailable.session && !unavailable.session->closed()) {
            unavailable.session->current_pool_.reset();
            unavailable.session->send_simple(http::status::service_unavailable,
                                             "worker unavailable", false,
                                             unavailable.request.version());
        }
    }
    arm_queue_timer(pool_key);
}

void ManagedListenerImpl::wire_pool(const std::shared_ptr<GenerationPool>& pool) {
    if (!pool) {
        return;
    }
    pool->set_shared_event_sink(
        [mailbox = mailbox_](std::shared_ptr<WorkerExecutor> executor, WorkerEvent event) {
            mailbox->post(std::move(executor), std::move(event));
        });
    std::weak_ptr<GenerationPool> weak_pool = pool;
    pool->set_scale_callbacks(
        [weak = weak_from_this(), mailbox = mailbox_, weak_pool] {
            mailbox->post_task([weak, weak_pool] {
                if (const auto self = weak.lock()) {
                    if (const auto target = weak_pool.lock()) {
                        self->pump_pool_queue(target.get());
                    }
                }
            });
        },
        [weak = weak_from_this(), mailbox = mailbox_, weak_pool](const std::string&) {
            mailbox->post_task([weak, weak_pool] {
                if (const auto self = weak.lock()) {
                    if (const auto target = weak_pool.lock()) {
                        self->fail_pool_queue_unavailable(target.get());
                    }
                }
            });
        });
}

void ManagedListenerImpl::arm_queue_timer(const GenerationPool* pool_key) {
    const auto q_it = pool_queues_.find(pool_key);
    if (q_it == pool_queues_.end()) {
        return;
    }
    PoolQueue& pq = q_it->second;
    if (pq.entries.empty()) {
        if (pq.timer) {
            pq.timer->cancel();
            pq.timer.reset();
        }
        pool_queues_.erase(q_it);
        return;
    }
    if (pq.queue_timeout_ms == 0 || !pq.entries.front().deadline) {
        if (pq.timer) {
            pq.timer->cancel();
            pq.timer.reset();
        }
        return;
    }
    if (!pq.timer) {
        pq.timer.emplace(mailbox_->ioc);
    }
    pq.timer->expires_at(*pq.entries.front().deadline);
    pq.timer->async_wait([weak = weak_from_this(), pool_key](const beast::error_code ec) {
        if (const std::shared_ptr<ManagedListenerImpl> self = weak.lock()) {
            self->on_queue_timer(pool_key, ec);
        }
    });
}

void ManagedListenerImpl::expire_front_queued(PoolQueue& pq) {
    QueuedRequest expired = std::move(pq.entries.front());
    pq.entries.pop_front();
    pq.queued_bytes -= expired.bytes;
    if (expired.pool && options_.metrics != nullptr) {
        options_.metrics->count_pool_rejection(std::string(expired.pool->application_id()),
                                               std::string(expired.pool->tenant_id()),
                                               "queue_timeout");
    }
    if (expired.session && !expired.session->closed()) {
        expired.session->current_pool_.reset();
        expired.session->send_simple(http::status::gateway_timeout, "queued request timeout",
                                     expired.request.keep_alive(), expired.request.version());
    }
}

void ManagedListenerImpl::on_queue_timer(const GenerationPool* pool_key,
                                         const beast::error_code ec) {
    if (ec == asio::error::operation_aborted || stop_requested_.load(std::memory_order_acquire)) {
        return;
    }
    const auto q_it = pool_queues_.find(pool_key);
    if (q_it == pool_queues_.end()) {
        return;
    }
    PoolQueue& pq = q_it->second;
    const auto now = std::chrono::steady_clock::now();
    while (!pq.entries.empty() && pq.entries.front().deadline &&
           now >= *pq.entries.front().deadline) {
        expire_front_queued(pq);
    }
    arm_queue_timer(pool_key);
}

void ManagedListenerImpl::cancel_queued_for_session(
    const GenerationPool* pool_key, const std::shared_ptr<ListenerSession>& session) {
    const auto q_it = pool_queues_.find(pool_key);
    if (q_it == pool_queues_.end()) {
        return;
    }
    PoolQueue& pq = q_it->second;
    for (auto it = pq.entries.begin(); it != pq.entries.end(); ++it) {
        if (it->session == session) {
            pq.queued_bytes -= it->bytes;
            pq.entries.erase(it);
            arm_queue_timer(pool_key);
            return;
        }
    }
}

void ManagedListenerImpl::begin_request(const std::shared_ptr<GenerationPool>& pool,
                                        GenerationPool::DispatchLease lease,
                                        std::uint64_t request_id,
                                        const std::shared_ptr<ListenerSession>& session,
                                        const http::request<http::string_body>& request,
                                        const NormalizedPublicRequest& normalized) {
    const std::shared_ptr<WorkerExecutor> executor = lease.executor();
    if (!executor) {
        session->current_request_id_.reset();
        session->current_executor_.reset();
        session->current_pool_.reset();
        session->send_simple(http::status::service_unavailable, "worker unavailable", false,
                             request.version());
        return;
    }
    PendingRequest pending;
    pending.pool = pool;
    std::vector<PublicRequestHeaderView> upgrade_views;
    for (const auto& field : request.base())
        upgrade_views.push_back({field.name_string(), field.value()});
    const bool upgrade =
        check_websocket_upgrade(upgrade_views, request.method_string(), request.version()) ==
        WebSocketUpgradeCheck::kValid;
    if (upgrade && !(executor->effective_channel_caps() & CAPSID_PROTOCOL_CAP_UPGRADE)) {
        session->current_request_id_.reset();
        session->current_executor_.reset();
        session->current_pool_.reset();
        session->send_simple(http::status::service_unavailable, "worker does not support WebSocket",
                             false, request.version());
        return;
    }
    pending.lease = std::make_shared<ChannelLease>(channel_ledger_, upgrade);
    if (!pending.lease->valid()) {
        session->current_request_id_.reset();
        session->current_executor_.reset();
        session->current_pool_.reset();
        session->send_simple(http::status::too_many_requests, "channel capacity exhausted", false,
                             request.version());
        return;
    }
    if (upgrade) {
        pending.upgrade_key = std::string(request[http::field::sec_websocket_key]);
        for (const auto& field : request.base())
            if (field.name() == http::field::sec_websocket_protocol) {
                if (!pending.upgrade_protocols.empty())
                    pending.upgrade_protocols += ",";
                pending.upgrade_protocols += std::string(field.value());
            }
    }
    pending.executor = executor;
    pending.session = session;
    pending.method = std::string(request.method_string());
    pending.keep_alive = request.keep_alive();
    pending.version = request.version();
    pending.request_body = request.body();
    const bool has_body = !pending.request_body.empty();

    Command begin;
    begin.type = CommandType::kBeginRequest;
    begin.request_id = request_id;
    begin.method = pending.method;
    begin.url = normalized.url;
    begin.headers = normalized.headers;
    begin.upgrade_request = upgrade;
    begin.channel_max_message = executor->channel_limits().max_message_bytes;
    if (upgrade)
        begin.headers.push_back({"upgrade", "websocket"});
    const bool send_fused_end = upgrade || (bodyless_enabled_ && !has_body);
    begin.end_request = send_fused_end;
    const std::uint64_t exec_id = executor->id();
    if (!lease.submit_begin(std::move(begin))) {
        session->current_request_id_.reset();
        session->current_executor_.reset();
        session->current_pool_.reset();
        session->send_simple(http::status::service_unavailable, "worker exited", false,
                             request.version());
        return;
    }
    pending.request_ended = send_fused_end && !has_body;
    requests_[{exec_id, request_id}] = std::move(pending);
}

void ManagedListenerImpl::handle_worker_event(std::shared_ptr<WorkerExecutor> executor,
                                              WorkerEvent event) {
    if (!executor) {
        return;
    }
    const std::uint64_t exec_id = executor->id();
    if (event.type == WorkerEvent::Type::kChannelData ||
        event.type == WorkerEvent::Type::kChannelCredit ||
        event.type == WorkerEvent::Type::kChannelClose ||
        event.type == WorkerEvent::Type::kChannelCancel ||
        event.type == WorkerEvent::Type::kChannelAck) {
        auto found = websockets_.find({exec_id, event.channel_id});
        if (found != websockets_.end()) {
            auto connection = found->second;
            connection->on_event(std::move(event));
        } else if (event.channel_id != 0 && (event.type == WorkerEvent::Type::kChannelCancel ||
                                             (event.type == WorkerEvent::Type::kChannelClose &&
                                              (event.close_kind == 2 || event.close_kind == 3)))) {
            Command ack;
            ack.type = CommandType::kChannelConfirm;
            ack.channel_id = event.channel_id;
            ack.channel_flags = (event.type == WorkerEvent::Type::kChannelCancel) ? 1U : 2U;
            executor->try_submit_channel(std::move(ack));
        }
        return;
    }
    switch (event.type) {
    case WorkerEvent::Type::kChannelDial:
    case WorkerEvent::Type::kChannelReady:
        return; // owned by the executor's dialer
    case WorkerEvent::Type::kChannelData:
    case WorkerEvent::Type::kChannelCredit:
    case WorkerEvent::Type::kChannelClose:
    case WorkerEvent::Type::kChannelCancel:
    case WorkerEvent::Type::kChannelAck:
        return;
    case WorkerEvent::Type::kRequestCredit: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        it->second.request_credit += event.credit;
        advance_request_body(executor, event.request_id);
        return;
    }
    case WorkerEvent::Type::kResponseHead: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        if (event.upgrade) {
            std::string handshake;
            if (pending.upgrade_key.empty() || !pending.session || pending.session->closed() ||
                !sanitize_response_headers(&event.headers) ||
                !websocket_handshake(pending.upgrade_key, pending.upgrade_protocols, event.headers,
                                     &handshake)) {
                Command cancel;
                cancel.type = CommandType::kChannelCancel;
                cancel.channel_id = event.channel_id;
                cancel.channel_flags = CAPSID_CANCEL_REASON_PROTOCOL_VIOLATION;
                executor->submit(std::move(cancel));
                reject_response_head(executor, event.request_id, "invalid WebSocket response");
                return;
            }
            auto session = pending.session;
            auto lease = pending.lease;
            const std::shared_ptr<GenerationPool> pool = pending.pool;
            lease->commit_upgrade();
            auto connection = std::make_shared<WebSocketSession>(
                session->stream_, session, lease, event.channel_id, executor->channel_limits(),
                options_.config.limits.stream_idle_timeout_ms,
                [executor](Command command) {
                    return executor->try_submit_channel(std::move(command));
                },
                [weak = weak_from_this(), exec_id, id = event.channel_id, session] {
                    if (auto alive = weak.lock()) {
                        alive->websockets_.erase({exec_id, id});
                        session->close();
                        alive->write_channel_metrics_line();
                    }
                },
                channel_budget_);
            session->websocket_ = connection;
            session->current_request_id_.reset();
            session->current_executor_.reset();
            std::vector<uint8_t> leftovers(session->buffer_.size());
            asio::buffer_copy(asio::buffer(leftovers), session->buffer_.data());
            session->buffer_.consume(leftovers.size());
            websockets_.emplace(std::make_pair(exec_id, event.channel_id), connection);
            requests_.erase(it);
            executor->retire_terminal_request(event.request_id);
            pump_pool_queue(pool.get());
            connection->start(std::move(handshake), std::move(leftovers));
            write_channel_metrics_line();
            return;
        }
        if (pending.lease)
            pending.lease->reject_upgrade();
        if (event.status < 200 || event.status > 599) {
            reject_response_head(executor, event.request_id, "invalid worker response status");
            return;
        }
        // Listener-level CORS is authoritative when configured: the App
        // cannot write its own Access-Control-Allow-Origin (that would
        // bypass the Host allow-list), and credentials survive only for an
        // exact allowed origin. A wildcard or a disallowed/absent Origin
        // strips App-owned credentials so wildcard never becomes any-origin
        // credentialed CORS. Vary is merged token-wise so `Vary:
        // Accept-Encoding` cannot suppress the required `Vary: Origin`.
        if (pending.session && pending.session->cors_) {
            pending.session->cors_->filter_headers(&event.headers);
        }
        if (!sanitize_response_headers(&event.headers)) {
            reject_response_head(executor, event.request_id, "invalid worker response headers");
            return;
        }
        bool is_sse = false;
        for (const auto& [name, value] : event.headers) {
            if (ascii_lower(name) == "content-type") {
                is_sse = is_event_stream_content_type(value);
                break;
            }
        }
        if (is_sse && !acquire_streaming_permit(exec_id, pending)) {
            reject_response_head(executor, event.request_id, "streaming capacity exhausted");
            return;
        }
        pending.head_only = pending.method == "HEAD";
        if (event.fixed_body) {
            pending.fixed_response =
                std::make_shared<http::response<http::vector_body<std::uint8_t>>>();
            pending.fixed_response->result(static_cast<http::status>(event.status));
            pending.fixed_response->version(pending.version);
            pending.fixed_response->keep_alive(pending.keep_alive);
            for (const auto& [name, value] : event.headers) {
                pending.fixed_response->base().insert(name, value);
            }
            const auto content_length_field =
                pending.fixed_response->base().find(http::field::content_length);
            if (content_length_field != pending.fixed_response->base().end()) {
                std::uint64_t value = 0;
                bool valid = true;
                const std::string_view text(content_length_field->value());
                if (text.empty()) {
                    valid = false;
                }
                for (const char c : text) {
                    if (c < '0' || c > '9') {
                        valid = false;
                        break;
                    }
                    const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
                    if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
                        valid = false;
                        break;
                    }
                    value = value * 10 + digit;
                }
                if (!valid || value != event.fixed_body_size) {
                    reject_response_head(executor, event.request_id,
                                         "fixed response Content-Length mismatch");
                    return;
                }
            }
            pending.fixed_body_expected = event.fixed_body_size;
            pending.fixed_body_received = 0;
            if (!pending.head_only) {
                pending.fixed_response->body().reserve(pending.fixed_body_expected);
            }
            return;
        }
        pending.response = std::make_shared<http::response<http::buffer_body>>();
        pending.response->result(static_cast<http::status>(event.status));
        pending.response->version(pending.version);
        pending.response->keep_alive(pending.keep_alive);
        for (const auto& [name, value] : event.headers) {
            pending.response->base().insert(name, value);
        }
        if (!pending.response->has_content_length() && !pending.response->chunked()) {
            pending.response->chunked(true);
        }
        const auto content_length_field =
            pending.response->base().find(http::field::content_length);
        if (content_length_field != pending.response->base().end()) {
            std::uint64_t value = 0;
            bool valid = true;
            const std::string_view text(content_length_field->value());
            if (text.empty()) {
                valid = false;
            }
            for (const char c : text) {
                if (c < '0' || c > '9') {
                    valid = false;
                    break;
                }
                const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
                if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) {
                    valid = false;
                    break;
                }
                value = value * 10 + digit;
            }
            if (!valid ||
                value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                reject_response_head(executor, event.request_id, "invalid worker Content-Length");
                return;
            }
            pending.cl_known = true;
            pending.cl_remaining = static_cast<std::size_t>(value);
        }
        pending.serializer =
            std::make_shared<http::response_serializer<http::buffer_body>>(*pending.response);
        pending.writing = true;
        arm_write_timer(event.request_id, pending);
        http::async_write_header(
            pending.session->stream(), *pending.serializer,
            [self = shared_from_this(), response = pending.response,
             serializer = pending.serializer, session = pending.session, executor,
             request_id = event.request_id](beast::error_code ec, std::size_t) {
                (void)response;
                (void)serializer;
                (void)session;
                auto it = self->requests_.find({executor->id(), request_id});
                if (it == self->requests_.end()) {
                    return;
                }
                PendingRequest& pending = it->second;
                pending.writing = false;
                self->disarm_write_timer(pending);
                if (ec) {
                    self->fail_request(executor, request_id, pending.session);
                    return;
                }
                pending.head_sent = true;
                if (pending.holds_streaming_permit && !pending.end_seen) {
                    self->arm_stream_idle_timer(executor, request_id, pending);
                }
                if (!pending.body_queue.empty()) {
                    PendingRequest::QueuedBody queued =
                        take_coalesced_response_body(&pending.body_queue, &pending.body_queue_bytes,
                                                     kResponseBodyWriteBatchLimit);
                    self->write_body_block(executor, request_id, std::move(queued.bytes),
                                           queued.credit_returned_early);
                } else if (pending.end_seen) {
                    if (pending.head_only) {
                        self->finalize_request(executor, request_id, pending.session);
                    } else {
                        self->write_end_block(executor, request_id);
                    }
                }
            });
        return;
    }
    case WorkerEvent::Type::kResponseBody: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        if (pending.holds_streaming_permit && !pending.end_seen) {
            arm_stream_idle_timer(executor, event.request_id, pending);
        }
        if (pending.fixed_response) {
            if (pending.fixed_body_received > pending.fixed_body_expected ||
                event.body.size() > pending.fixed_body_expected - pending.fixed_body_received) {
                fail_request(executor, event.request_id, pending.session);
                return;
            }
            pending.fixed_body_received += event.body.size();
            if (!pending.head_only) {
                mailbox_->add_client_bytes(exec_id, event.body.size());
                pending.fixed_response->body().insert(pending.fixed_response->body().end(),
                                                      event.body.begin(), event.body.end());
            }
            return;
        }
        // Response credit (spec §8.1): return credit as soon as the frame
        // is received, not after the client write completes, while the
        // per-request write queue stays shallow — one fewer host
        // round-trip per frame. Beyond the window credit falls back to
        // write-completion, so a slow client cannot make the host buffer
        // unboundedly.
        const bool credit_returned_early = pending.body_queue_bytes < kEarlyCreditWindow;
        if (credit_returned_early) {
            pending.pending_response_credit += static_cast<std::uint32_t>(event.body.size());
            flush_pending_credit(executor, event.request_id, pending, false);
        }
        if (pending.head_only) {
            // HEAD consumes the worker body without exposing it.
            return;
        }
        // Client-bytes accounting: counted once at arrival (queued or
        // written directly), returned on the write completion.
        mailbox_->add_client_bytes(exec_id, event.body.size());
        if (pending.writing) {
            pending.body_queue_bytes += event.body.size();
            pending.body_queue.push_back(
                PendingRequest::QueuedBody{std::move(event.body), credit_returned_early});
            return;
        }
        write_body_block(executor, event.request_id, std::move(event.body), credit_returned_early);
        return;
    }
    case WorkerEvent::Type::kResponseEnd: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        pending.end_seen = true;
        if (pending.idle_timer) {
            pending.idle_timer->cancel();
            pending.idle_timer.reset();
        }
        // Drain any remaining pending credit; the Runtime erased the
        // request, so the grant frame is queued only to satisfy the
        // terminal retirement.
        flush_pending_credit(executor, event.request_id, pending, true);
        // The Runtime erased this request with RESPONSE_END. Purge queued
        // stale frames and retire the tombstone after the worker's current
        // command batch, without a redundant Runtime cancel.
        const std::shared_ptr<GenerationPool> pool = pending.pool;
        executor->retire_terminal_request(event.request_id);
        pump_pool_queue(pool.get());
        auto still_it = requests_.find({exec_id, event.request_id});
        if (still_it == requests_.end()) {
            return;
        }
        PendingRequest& active_pending = still_it->second;
        if (active_pending.fixed_response) {
            if (active_pending.fixed_body_received != active_pending.fixed_body_expected) {
                fail_request(executor, event.request_id, active_pending.session);
                return;
            }
            write_fixed_response(executor, event.request_id);
            return;
        }
        if (active_pending.head_only) {
            if (!active_pending.writing) {
                finalize_request(executor, event.request_id, active_pending.session);
            }
            return;
        }
        if (!active_pending.writing) {
            write_end_block(executor, event.request_id);
        }
        return;
    }
    case WorkerEvent::Type::kLog:
        if (options_.log != nullptr) {
            LogFields fields;
            fields.level = event.binding_log ? event.log_level : "info";
            fields.event = log_events::kAppLog;
            fields.app = event.application_id;
            fields.generation = event.generation_digest;
            fields.binding = event.binding_id;
            if (event.request_id != 0) {
                fields.request_id = std::to_string(event.request_id);
            }
            fields.fields = event.log_fields_json;
            fields.message = event.text;
            options_.log->log(LogLane::kApp, std::move(fields));
        } else {
            write_stderr(event.text);
        }
        return;
    case WorkerEvent::Type::kError:
        write_stderr(std::string("capsid-host: worker error: ") + event.text + " (request " +
                     std::to_string(event.request_id) + ")");
        return;
    case WorkerEvent::Type::kRequestTimeout: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        const std::shared_ptr<ListenerSession> session = pending.session;
        const bool head_sent = pending.head_sent;
        const bool writing = pending.writing;
        const bool keep_alive = pending.keep_alive;
        const unsigned version = pending.version;
        cancel_request(executor, event.request_id);
        // Once any response byte reached the wire, only a connection close
        // is legal (a fresh response would corrupt the stream).
        if (head_sent || writing) {
            if (session && !session->closed()) {
                session->close();
            }
        } else if (session && !session->closed()) {
            session->send_simple(http::status::gateway_timeout, "worker request timeout",
                                 keep_alive, version);
        }
        return;
    }
    case WorkerEvent::Type::kRequestFailure: {
        auto it = requests_.find({exec_id, event.request_id});
        if (it == requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        const std::shared_ptr<ListenerSession> session = pending.session;
        const bool head_sent = pending.head_sent;
        const bool writing = pending.writing;
        const bool keep_alive = pending.keep_alive;
        const unsigned version = pending.version;
        cancel_request(executor, event.request_id);
        if (head_sent || writing) {
            if (session && !session->closed()) {
                session->close();
            }
        } else if (session && !session->closed()) {
            session->send_simple(http::status::service_unavailable, "worker request failed",
                                 keep_alive, version);
        }
        return;
    }
    case WorkerEvent::Type::kExit: {
        std::vector<std::shared_ptr<WebSocketSession>> abandoned;
        for (auto& item : websockets_)
            if (item.first.first == exec_id)
                abandoned.push_back(item.second);
        for (auto& connection : abandoned)
            connection->abort(false);
        // A worker died: every request pinned to this executor is gone with
        // it (the executor emits no per-request failure for an exit). The
        // pool handles the replacement — the listener only fails the
        // requests, and keeps accepting: a replacement worker serves the
        // next request.
        const std::vector<std::pair<std::uint64_t, std::uint64_t>> pinned = [this, exec_id] {
            std::vector<std::pair<std::uint64_t, std::uint64_t>> ids;
            for (const auto& entry : requests_) {
                if (entry.first.first == exec_id) {
                    ids.push_back(entry.first);
                }
            }
            return ids;
        }();
        for (const auto& key : pinned) {
            auto it = requests_.find(key);
            if (it == requests_.end()) {
                continue;
            }
            PendingRequest& pending = it->second;
            const std::shared_ptr<ListenerSession> session = pending.session;
            const bool head_sent = pending.head_sent;
            const bool writing = pending.writing;
            const unsigned version = pending.version;
            cancel_request(executor, key.second);
            if (head_sent || writing) {
                if (session && !session->closed()) {
                    session->close();
                }
            } else if (session && !session->closed()) {
                session->send_simple(http::status::service_unavailable, "worker exited", false,
                                     version);
            }
        }
        next_request_id_.erase(exec_id);
        streaming_inflight_.erase(exec_id);
        mailbox_->erase_executor(exec_id);
        return;
    }
    }
}

void ManagedListenerImpl::advance_request_body(const std::shared_ptr<WorkerExecutor>& executor,
                                               std::uint64_t request_id) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    PendingRequest& pending = it->second;
    // One write per credit event: the worker replenishes request credit
    // only after consuming the previous chunk, so submitting several chunks
    // at once would race the worker's window and be rejected as an invalid
    // frame.
    if (pending.request_credit > 0 && pending.request_body_offset < pending.request_body.size()) {
        const std::size_t remaining = pending.request_body.size() - pending.request_body_offset;
        const std::size_t chunk =
            static_cast<std::size_t>(std::min<std::uint64_t>(pending.request_credit, remaining));
        Command command;
        command.type = CommandType::kWriteRequest;
        command.request_id = request_id;
        command.body.assign(pending.request_body.data() + pending.request_body_offset,
                            pending.request_body.data() + pending.request_body_offset + chunk);
        executor->submit(std::move(command));
        pending.request_credit -= chunk;
        pending.request_body_offset += chunk;
    }
    if (pending.request_body_offset == pending.request_body.size() && !pending.request_ended) {
        pending.request_ended = true;
        if (!pending.end_seen) {
            Command command;
            command.type = CommandType::kEndRequest;
            command.request_id = request_id;
            executor->submit(std::move(command));
        }
    }
}

void ManagedListenerImpl::write_body_block(const std::shared_ptr<WorkerExecutor>& executor,
                                           std::uint64_t request_id,
                                           std::vector<std::uint8_t> bytes,
                                           bool credit_returned_early) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    PendingRequest& pending = it->second;
    if (pending.cl_known) {
        if (bytes.size() > pending.cl_remaining) {
            // The worker produced more body than its declared
            // Content-Length: fail the connection closed.
            fail_request(executor, request_id, pending.session);
            return;
        }
        pending.cl_remaining -= bytes.size();
    }
    pending.outgoing = std::move(bytes);
    pending.outgoing_credit_returned_early = credit_returned_early;
    pending.writing = true;
    pending.response->body().data = pending.outgoing.data();
    pending.response->body().size = pending.outgoing.size();
    pending.response->body().more = true;
    arm_write_timer(request_id, pending);
    http::async_write(
        pending.session->stream(), *pending.serializer,
        [self = shared_from_this(), response = pending.response, serializer = pending.serializer,
         session = pending.session, executor, request_id,
         bytes = pending.outgoing.size()](beast::error_code ec, std::size_t) {
            (void)response;
            (void)serializer;
            (void)session;
            // The client-bytes term is returned on the write completion
            // regardless of the request state (a cancel erases the entry
            // but the write still completes).
            self->mailbox_->sub_client_bytes(executor->id(), bytes);
            auto it = self->requests_.find({executor->id(), request_id});
            if (it == self->requests_.end()) {
                return;
            }
            PendingRequest& pending = it->second;
            pending.writing = false;
            self->disarm_write_timer(pending);
            if (ec && ec != http::error::need_buffer) {
                self->fail_request(executor, request_id, pending.session);
                return;
            }
            // need_buffer is the buffer_body serializer's normal signal that
            // the current block was fully written and more data is expected.
            // Credit is returned only after the client write succeeded, for
            // exactly the bytes that were written (early-credited blocks
            // were reimbursed at receive time and must not be doubled).
            // Once the response ended, the Runtime erased the request and
            // the credit is moot; submitting the frame would only be
            // rejected, so it is skipped.
            if (!pending.end_seen && !pending.outgoing_credit_returned_early) {
                pending.pending_response_credit += static_cast<std::uint32_t>(bytes);
                const bool force_credit = pending.body_queue.empty();
                self->flush_pending_credit(executor, request_id, pending, force_credit);
            }
            pending.outgoing_credit_returned_early = false;
            pending.outgoing.clear();
            if (!pending.body_queue.empty()) {
                PendingRequest::QueuedBody queued = take_coalesced_response_body(
                    &pending.body_queue, &pending.body_queue_bytes, kResponseBodyWriteBatchLimit);
                self->write_body_block(executor, request_id, std::move(queued.bytes),
                                       queued.credit_returned_early);
            } else if (pending.end_seen) {
                self->write_end_block(executor, request_id);
            }
        });
}

// Spec §8.1: submit accumulated response credit in >=16 KiB batches mid-stream,
// or force-flush smaller tails at write completion / end.
void ManagedListenerImpl::flush_pending_credit(const std::shared_ptr<WorkerExecutor>& executor,
                                               std::uint64_t request_id, PendingRequest& pending,
                                               bool force) {
    if (pending.pending_response_credit == 0 ||
        (credit_grant_threshold_ > 0 && !force &&
         pending.pending_response_credit < credit_grant_threshold_)) {
        return;
    }
    Command grant;
    grant.type = CommandType::kGrantResponseCredit;
    grant.request_id = request_id;
    grant.credit = pending.pending_response_credit;
    pending.pending_response_credit = 0;
    executor->submit(std::move(grant));
}

void ManagedListenerImpl::write_fixed_response(const std::shared_ptr<WorkerExecutor>& executor,
                                               std::uint64_t request_id) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    PendingRequest& pending = it->second;
    pending.fixed_response->content_length(pending.fixed_body_expected);
    pending.fixed_serializer =
        std::make_shared<http::response_serializer<http::vector_body<std::uint8_t>>>(
            *pending.fixed_response);
    pending.fixed_write_started = true;
    pending.writing = true;
    arm_write_timer(request_id, pending);
    const std::size_t bytes = pending.head_only ? 0 : pending.fixed_body_received;
    const auto completion = [self = shared_from_this(), response = pending.fixed_response,
                             serializer = pending.fixed_serializer, session = pending.session,
                             executor, request_id, bytes](beast::error_code ec, std::size_t) {
        (void)response;
        (void)serializer;
        (void)session;
        self->mailbox_->sub_client_bytes(executor->id(), bytes);
        auto it = self->requests_.find({executor->id(), request_id});
        if (it == self->requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        pending.writing = false;
        self->disarm_write_timer(pending);
        if (ec) {
            self->fail_request(executor, request_id, pending.session);
            return;
        }
        pending.head_sent = true;
        self->finalize_request(executor, request_id, pending.session);
    };
    if (pending.head_only) {
        http::async_write_header(pending.session->stream(), *pending.fixed_serializer, completion);
    } else {
        http::async_write(pending.session->stream(), *pending.fixed_serializer, completion);
    }
}

void ManagedListenerImpl::write_end_block(const std::shared_ptr<WorkerExecutor>& executor,
                                          std::uint64_t request_id) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    PendingRequest& pending = it->second;
    if (pending.cl_known && pending.cl_remaining > 0) {
        // The worker ended the response short of its declared
        // content-length; the client would wait forever.
        fail_request(executor, request_id, pending.session);
        return;
    }
    pending.writing = true;
    pending.response->body().data = nullptr;
    pending.response->body().size = 0;
    pending.response->body().more = false;
    arm_write_timer(request_id, pending);
    http::async_write(pending.session->stream(), *pending.serializer,
                      [self = shared_from_this(), response = pending.response,
                       serializer = pending.serializer, session_ref = pending.session, executor,
                       request_id](beast::error_code ec, std::size_t) {
                          (void)response;
                          (void)serializer;
                          (void)session_ref;
                          auto it = self->requests_.find({executor->id(), request_id});
                          if (it == self->requests_.end()) {
                              return;
                          }
                          PendingRequest& pending = it->second;
                          pending.writing = false;
                          self->disarm_write_timer(pending);
                          if (ec) {
                              self->fail_request(executor, request_id, pending.session);
                              return;
                          }
                          self->release_streaming_permit(pending);
                          const bool keep_alive = pending.keep_alive;
                          std::shared_ptr<ListenerSession> session = pending.session;
                          const std::shared_ptr<GenerationPool> pool = pending.pool;
                          self->requests_.erase(it);
                          if (session && !session->closed()) {
                              if (keep_alive) {
                                  session->read_head();
                              } else {
                                  session->close();
                              }
                          }
                          self->pump_pool_queue(pool.get());
                          self->write_channel_metrics_line();
                      });
}

bool ManagedListenerImpl::response_may_complete_early(std::uint64_t executor_id,
                                                      std::uint64_t request_id) const {
    const auto it = requests_.find({executor_id, request_id});
    if (it == requests_.end())
        return false;
    const PendingRequest& pending = it->second;
    if (pending.fixed_response)
        return pending.end_seen && pending.fixed_body_received == pending.fixed_body_expected;
    if (pending.head_only)
        return pending.writing || pending.head_sent;
    if (pending.cl_known)
        return pending.cl_remaining == 0;
    return pending.end_seen && pending.response && !pending.response->body().more;
}

void ManagedListenerImpl::write_channel_metrics_line() {
    if (!options_.emit_channel_metrics) {
        return;
    }
    const std::string line =
        "capsid-host: channel-metrics {\"service\":" +
        channel_budget_metrics_json(service_channel_budget_->snapshot()) +
        ",\"listener\":" + channel_budget_metrics_json(channel_budget_->snapshot()) + "}\n";
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
}

void ManagedListenerImpl::finalize_request(const std::shared_ptr<WorkerExecutor>& executor,
                                           std::uint64_t request_id,
                                           std::shared_ptr<ListenerSession> session) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    release_streaming_permit(it->second);
    const bool keep_alive = it->second.keep_alive;
    const std::shared_ptr<GenerationPool> pool = it->second.pool;
    requests_.erase(it);
    if (session && !session->closed()) {
        if (keep_alive) {
            session->read_head();
        } else {
            session->close();
        }
    }
    pump_pool_queue(pool.get());
    write_channel_metrics_line();
}

void ManagedListenerImpl::cancel_request(const std::shared_ptr<WorkerExecutor>& executor,
                                         std::uint64_t request_id) {
    if (!executor) {
        return;
    }
    const std::uint64_t exec_id = executor->id();
    auto it = requests_.find({exec_id, request_id});
    if (it == requests_.end()) {
        return;
    }
    release_streaming_permit(it->second);
    // Queued response bytes never reach the wire; return their client-bytes
    // accounting (the in-flight write returns its own on completion).
    mailbox_->sub_client_bytes(exec_id, it->second.body_queue_bytes);
    if (it->second.fixed_response && !it->second.fixed_write_started && !it->second.head_only) {
        mailbox_->sub_client_bytes(exec_id, it->second.fixed_body_received);
    }
    const std::shared_ptr<GenerationPool> pool = it->second.pool;
    requests_.erase(it);
    executor->mark_canceled(request_id);
    Command cancel;
    cancel.type = CommandType::kCancel;
    cancel.request_id = request_id;
    executor->submit(std::move(cancel));
    pump_pool_queue(pool.get());
    write_channel_metrics_line();
}

void ManagedListenerImpl::fail_request(const std::shared_ptr<WorkerExecutor>& executor,
                                       std::uint64_t request_id,
                                       std::shared_ptr<ListenerSession> session) {
    cancel_request(executor, request_id);
    if (session && !session->closed()) {
        session->close();
    }
}

void ManagedListenerImpl::reject_response_head(const std::shared_ptr<WorkerExecutor>& executor,
                                               std::uint64_t request_id,
                                               const std::string& reason) {
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    const std::shared_ptr<ListenerSession> session = it->second.session;
    const bool keep_alive = it->second.keep_alive;
    const unsigned version = it->second.version;
    cancel_request(executor, request_id);
    if (session && !session->closed()) {
        // Worker/IPC failure before the response head → 503.
        session->send_simple(http::status::service_unavailable, reason, keep_alive, version);
    }
}

bool ManagedListenerImpl::sanitize_response_headers(
    std::vector<std::pair<std::string, std::string>>* headers) {
    std::set<std::string> nominated;
    unsigned content_length_count = 0;
    for (const auto& [name, value] : *headers) {
        if (name.empty()) {
            return false;
        }
        for (const unsigned char c : name) {
            if (!is_token_char(c)) {
                return false;
            }
        }
        for (const unsigned char c : value) {
            if (!valid_field_value_byte(c)) {
                return false;
            }
        }
        const std::string lower = ascii_lower(name);
        if (lower == "connection") {
            if (!collect_connection_nominations(value, &nominated)) {
                return false;
            }
        } else if (lower == "content-length") {
            ++content_length_count;
            if (content_length_count > 1 || value.empty()) {
                return false;
            }
            for (const unsigned char c : value) {
                if (c < '0' || c > '9') {
                    return false;
                }
            }
        }
    }
    std::vector<std::pair<std::string, std::string>> sanitized;
    sanitized.reserve(headers->size());
    for (const auto& [name, value] : *headers) {
        const std::string lower = ascii_lower(name);
        if (lower == "connection" || lower == "keep-alive" || lower == "proxy-connection" ||
            lower == "transfer-encoding" || lower == "te" || lower == "trailer" ||
            lower == "upgrade" || lower == "proxy-authenticate" || lower == "proxy-authorization" ||
            nominated.count(lower) != 0) {
            continue; // hop-by-hop: never reaches the client
        }
        sanitized.push_back({name, value});
    }
    *headers = std::move(sanitized);
    return true;
}

bool ManagedListenerImpl::acquire_streaming_permit(std::uint64_t executor_id,
                                                   PendingRequest& pending) {
    std::uint64_t max_sse = 2;
    if (pending.pool && pending.pool->max_streaming_inflight_per_worker() != 0) {
        max_sse = pending.pool->max_streaming_inflight_per_worker();
    }
    if (pending.pool && pending.pool->max_inflight_per_worker() == 1 && max_sse > 1) {
        max_sse = 1;
    }
    std::uint64_t& active = streaming_inflight_[executor_id];
    if (active >= max_sse) {
        return false;
    }
    ++active;
    pending.holds_streaming_permit = true;
    return true;
}

void ManagedListenerImpl::release_streaming_permit(PendingRequest& pending) {
    if (!pending.holds_streaming_permit) {
        return;
    }
    pending.holds_streaming_permit = false;
    if (pending.idle_timer) {
        pending.idle_timer->cancel();
        pending.idle_timer.reset();
    }
    if (!pending.executor) {
        return;
    }
    auto it = streaming_inflight_.find(pending.executor->id());
    if (it != streaming_inflight_.end()) {
        if (it->second <= 1) {
            streaming_inflight_.erase(it);
        } else {
            --it->second;
        }
    }
}

std::uint64_t
ManagedListenerImpl::resolve_stream_idle_timeout_ms(const PendingRequest& pending) const {
    if (pending.pool && pending.pool->stream_idle_timeout_ms() != 0) {
        return pending.pool->stream_idle_timeout_ms();
    }
    if (options_.config.limits.stream_idle_timeout_ms != 0) {
        return options_.config.limits.stream_idle_timeout_ms;
    }
    return 60000;
}

std::uint64_t ManagedListenerImpl::resolve_write_timeout_ms(const PendingRequest& pending) const {
    if (pending.pool && pending.pool->write_timeout_ms() != 0) {
        return pending.pool->write_timeout_ms();
    }
    if (options_.config.limits.write_timeout_ms != 0) {
        return options_.config.limits.write_timeout_ms;
    }
    return options_.config.limits.stream_idle_timeout_ms;
}

void ManagedListenerImpl::arm_stream_idle_timer(const std::shared_ptr<WorkerExecutor>& executor,
                                                std::uint64_t request_id, PendingRequest& pending) {
    const std::uint64_t ms = resolve_stream_idle_timeout_ms(pending);
    if (ms == 0) {
        return;
    }
    if (!pending.idle_timer) {
        pending.idle_timer.emplace(mailbox_->ioc);
    }
    pending.idle_timer->expires_after(std::chrono::milliseconds(ms));
    pending.idle_timer->async_wait(
        [self = shared_from_this(), executor, request_id](const beast::error_code ec) {
            self->on_stream_idle_timer(executor, request_id, ec);
        });
}

void ManagedListenerImpl::on_stream_idle_timer(const std::shared_ptr<WorkerExecutor>& executor,
                                               std::uint64_t request_id,
                                               const beast::error_code ec) {
    if (ec || !executor) {
        return;
    }
    auto it = requests_.find({executor->id(), request_id});
    if (it == requests_.end()) {
        return;
    }
    const std::shared_ptr<ListenerSession> session = it->second.session;
    cancel_request(executor, request_id);
    if (session && !session->closed()) {
        session->close();
    }
}

void ManagedListenerImpl::arm_write_timer(std::uint64_t request_id, PendingRequest& pending) {
    const std::uint64_t ms = resolve_write_timeout_ms(pending);
    if (ms == 0) {
        return;
    }
    if (!pending.write_timer) {
        pending.write_timer.emplace(mailbox_->ioc);
    }
    pending.write_timer->expires_after(std::chrono::milliseconds(ms));
    pending.write_timer->async_wait([self = shared_from_this(), executor = pending.executor,
                                     request_id](const beast::error_code ec) {
        if (ec || !executor) {
            return; // cancelled by the write completion
        }
        auto it = self->requests_.find({executor->id(), request_id});
        if (it == self->requests_.end()) {
            return;
        }
        PendingRequest& pending = it->second;
        const std::shared_ptr<ListenerSession> session = pending.session;
        // The head reached the wire before this timer was armed, so
        // after the cancel only a connection close is legal.
        self->cancel_request(executor, request_id);
        if (session && !session->closed()) {
            session->close();
        }
    });
}

void ManagedListenerImpl::disarm_write_timer(PendingRequest& pending) {
    if (pending.write_timer) {
        pending.write_timer->cancel();
    }
}

// --------------------------------------------------------------------------
// Facade
// --------------------------------------------------------------------------

ManagedListener::ManagedListener(ManagedListenerOptions options)
    : impl_(std::make_shared<ManagedListenerImpl>(std::move(options))) {}

ManagedListener::~ManagedListener() {
    impl_->request_stop();
    std::string error;
    impl_->wait(&error);
}

bool ManagedListener::start(std::string* error) { return impl_->start(error); }

void ManagedListener::wire_pool(const std::shared_ptr<GenerationPool>& pool) {
    impl_->wire_pool(pool);
}

void ManagedListener::stop_accepting() { impl_->stop_accepting(); }

void ManagedListener::request_stop() { impl_->request_stop(); }

void ManagedListener::request_stop(std::function<void()> on_io_stopped) {
    impl_->request_stop(std::move(on_io_stopped));
}

bool ManagedListener::wait(std::string* error) { return impl_->wait(error); }

std::uint16_t ManagedListener::bound_port() const { return impl_->bound_port(); }

bool ManagedListener::running() const {
    return !impl_->stop_requested_.load(std::memory_order_relaxed);
}

} // namespace capsid::host
