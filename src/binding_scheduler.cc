// Binding RPC v2 (v0.4.0): fair round-robin multi-runtime scheduler and
// immediate shared-loop wake coordinator.

#include "binding_scheduler.h"

#include <cstring>

namespace capsid {

BindingScheduler::BindingScheduler(const BindingSchedulerConfig& config) : config_(config) {
    std::memset(&prepare_, 0, sizeof(prepare_));
    std::memset(&idle_, 0, sizeof(idle_));
    std::memset(&check_, 0, sizeof(check_));
}

BindingScheduler::~BindingScheduler() { detach(); }

void BindingScheduler::configure_bindings(std::size_t binding_count) {
    call_queues_.clear();
    call_queues_.resize(binding_count);
    rr_cursor_ = 0;
    total_queued_calls_ = 0;
}

bool BindingScheduler::attach(uv_loop_t* loop, TurnCallback callback, void* opaque) {
    if (loop == nullptr || callback == nullptr || attached_ || handles_initialized_) {
        return false;
    }
    if (uv_prepare_init(loop, &prepare_) != 0) {
        return false;
    }
    if (uv_idle_init(loop, &idle_) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(&prepare_), nullptr);
        return false;
    }
    if (uv_check_init(loop, &check_) != 0) {
        uv_close(reinterpret_cast<uv_handle_t*>(&idle_), nullptr);
        uv_close(reinterpret_cast<uv_handle_t*>(&prepare_), nullptr);
        return false;
    }
    handles_initialized_ = true;
    callback_ = callback;
    opaque_ = opaque;
    prepare_.data = this;
    idle_.data = this;
    check_.data = this;

    if (uv_prepare_start(&prepare_, on_prepare) != 0 || uv_check_start(&check_, on_check) != 0) {
        detach();
        return false;
    }
    uv_unref(reinterpret_cast<uv_handle_t*>(&prepare_));
    uv_unref(reinterpret_cast<uv_handle_t*>(&idle_));
    uv_unref(reinterpret_cast<uv_handle_t*>(&check_));
    attached_ = true;
    update_idle_state();
    return true;
}

void BindingScheduler::detach() {
    attached_ = false;
    callback_ = nullptr;
    opaque_ = nullptr;
    if (!handles_initialized_) {
        return;
    }
    handles_initialized_ = false;
    if (idle_active_) {
        uv_idle_stop(&idle_);
        idle_active_ = false;
    }
    uv_prepare_stop(&prepare_);
    uv_check_stop(&check_);
    prepare_.data = nullptr;
    idle_.data = nullptr;
    check_.data = nullptr;
    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&prepare_))) {
        uv_close(reinterpret_cast<uv_handle_t*>(&prepare_), nullptr);
    }
    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&idle_))) {
        uv_close(reinterpret_cast<uv_handle_t*>(&idle_), nullptr);
    }
    if (!uv_is_closing(reinterpret_cast<uv_handle_t*>(&check_))) {
        uv_close(reinterpret_cast<uv_handle_t*>(&check_), nullptr);
    }
}

void BindingScheduler::enqueue_call(std::size_t table_index, std::uint64_t call_id) {
    if (table_index >= call_queues_.size()) {
        call_queues_.resize(table_index + 1);
    }
    call_queues_[table_index].push_back(call_id);
    ++total_queued_calls_;
    update_idle_state();
}

bool BindingScheduler::cancel_call(std::size_t table_index, std::uint64_t call_id) {
    if (table_index >= call_queues_.size()) {
        return false;
    }
    std::deque<std::uint64_t>& queue = call_queues_[table_index];
    for (std::deque<std::uint64_t>::iterator it = queue.begin(); it != queue.end(); ++it) {
        if (*it == call_id) {
            queue.erase(it);
            if (total_queued_calls_ > 0) {
                --total_queued_calls_;
            }
            return true;
        }
    }
    return false;
}

bool BindingScheduler::pop_next_call(std::uint64_t* out_call_id) {
    if (total_queued_calls_ == 0 || call_queues_.empty()) {
        return false;
    }
    const std::size_t count = call_queues_.size();
    for (std::size_t scanned = 0; scanned < count; ++scanned) {
        const std::size_t idx = (rr_cursor_ + scanned) % count;
        if (!call_queues_[idx].empty()) {
            const std::uint64_t id = call_queues_[idx].front();
            call_queues_[idx].pop_front();
            --total_queued_calls_;
            rr_cursor_ = (idx + 1) % count;
            if (out_call_id != nullptr) {
                *out_call_id = id;
            }
            return true;
        }
    }
    total_queued_calls_ = 0;
    return false;
}

void BindingScheduler::enqueue_result(std::uint64_t call_id) {
    result_queue_.push_back(call_id);
    update_idle_state();
}

bool BindingScheduler::pop_next_result(std::uint64_t* out_call_id) {
    if (result_queue_.empty()) {
        return false;
    }
    const std::uint64_t id = result_queue_.front();
    result_queue_.pop_front();
    if (out_call_id != nullptr) {
        *out_call_id = id;
    }
    return true;
}

void BindingScheduler::enqueue_control(std::uint64_t item_id) {
    control_queue_.push_back(item_id);
    update_idle_state();
}

bool BindingScheduler::pop_next_control(std::uint64_t* out_item_id) {
    if (control_queue_.empty()) {
        return false;
    }
    const std::uint64_t id = control_queue_.front();
    control_queue_.pop_front();
    if (out_item_id != nullptr) {
        *out_item_id = id;
    }
    return true;
}

void BindingScheduler::wake() {
    if (attached_ || in_turn_) {
        reschedule_needed_ = true;
    }
    update_idle_state();
}

void BindingScheduler::clear_queues() {
    for (std::size_t i = 0; i < call_queues_.size(); ++i) {
        call_queues_[i].clear();
    }
    total_queued_calls_ = 0;
    result_queue_.clear();
    control_queue_.clear();
    reschedule_needed_ = false;
    update_idle_state();
}

bool BindingScheduler::begin_turn() {
    if (in_turn_) {
        reschedule_needed_ = true;
        update_idle_state();
        return false;
    }
    in_turn_ = true;
    reschedule_needed_ = false;
    return true;
}

void BindingScheduler::end_turn() {
    in_turn_ = false;
    update_idle_state();
}

void BindingScheduler::run_turn() {
    if (!attached_ || callback_ == nullptr) {
        return;
    }
    if (!has_pending_work()) {
        update_idle_state();
        return;
    }
    if (!begin_turn()) {
        return;
    }
    TurnCallback cb = callback_;
    void* opaque = opaque_;
    cb(opaque);
    end_turn();
}

void BindingScheduler::on_prepare(uv_prepare_t* handle) {
    BindingScheduler* self = static_cast<BindingScheduler*>(handle->data);
    if (self != nullptr) {
        self->update_idle_state();
    }
}

void BindingScheduler::on_idle(uv_idle_t* handle) {
    (void)handle;
    // Keeping the idle handle active only prevents uv_run from blocking in
    // epoll_wait while RPC work is pending; actual dispatch runs in on_check.
}

void BindingScheduler::on_check(uv_check_t* handle) {
    BindingScheduler* self = static_cast<BindingScheduler*>(handle->data);
    if (self != nullptr) {
        self->run_turn();
    }
}

void BindingScheduler::update_idle_state() {
    if (!attached_ || !handles_initialized_) {
        return;
    }
    const bool need_idle = has_pending_work();
    if (need_idle && !idle_active_) {
        if (uv_idle_start(&idle_, on_idle) == 0) {
            idle_active_ = true;
        }
    } else if (!need_idle && idle_active_) {
        uv_idle_stop(&idle_);
        idle_active_ = false;
    }
}

} // namespace capsid
