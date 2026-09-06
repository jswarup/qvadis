#include "qemu_event_dispatcher.h"

namespace qvadis {

QemuEventDispatcher::QemuEventDispatcher() = default;
QemuEventDispatcher::~QemuEventDispatcher() = default;

qemu_status_t QemuEventDispatcher::Subscribe(
    qemu_event_type_t event_type,
    qemu_event_callback_t callback,
    void* user_data
) {
    if (!callback) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_.push_back({event_type, callback, user_data});
    return QEMU_OK;
}

qemu_status_t QemuEventDispatcher::Unsubscribe(
    qemu_event_type_t event_type,
    qemu_event_callback_t callback
) {
    if (!callback) return QEMU_ERR_INVALID_ARG;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::remove_if(subscribers_.begin(), subscribers_.end(),
        [event_type, callback](const EventSubscription& sub) {
            return sub.type == event_type && sub.callback == callback;
        });

    if (it == subscribers_.end()) {
        return QEMU_ERR_NOT_FOUND;
    }

    subscribers_.erase(it, subscribers_.end());
    return QEMU_OK;
}

void QemuEventDispatcher::Dispatch(qemu_event_type_t type, const char* message, void* context) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    uint64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    message_storage_.emplace_back(message ? message : "");
    const char* stored_msg = message_storage_.back().c_str();

    qemu_event_t evt = {};
    evt.type = type;
    evt.timestamp_ns = ns;
    evt.context = context;
    evt.message = stored_msg;

    pending_events_.push(evt);
}

void QemuEventDispatcher::ProcessPendingEvents() {
    std::vector<qemu_event_t> events_to_process;
    std::vector<EventSubscription> subs_copy;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!pending_events_.empty()) {
            events_to_process.push_back(pending_events_.front());
            pending_events_.pop();
        }
        subs_copy = subscribers_;
    }

    for (const auto& evt : events_to_process) {
        for (const auto& sub : subs_copy) {
            if (sub.type == evt.type && sub.callback) {
                sub.callback(&evt, sub.user_data);
            }
        }
    }
}

size_t QemuEventDispatcher::GetSubscriberCount(qemu_event_type_t type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& s : subscribers_) {
        if (s.type == type) count++;
    }
    return count;
}

} // namespace qvadis
