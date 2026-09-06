#pragma once

#include "qemu_runtime.h"
#include <vector>
#include <mutex>
#include <queue>
#include <chrono>
#include <unordered_map>
#include <algorithm>

namespace qvadis {

struct EventSubscription {
    qemu_event_type_t type;
    qemu_event_callback_t callback;
    void* user_data;
};

class QemuEventDispatcher {
public:
    QemuEventDispatcher();
    ~QemuEventDispatcher();

    qemu_status_t Subscribe(qemu_event_type_t event_type, qemu_event_callback_t callback, void* user_data);
    qemu_status_t Unsubscribe(qemu_event_type_t event_type, qemu_event_callback_t callback);

    void Dispatch(qemu_event_type_t type, const char* message, void* context = nullptr);
    void ProcessPendingEvents();

    size_t GetSubscriberCount(qemu_event_type_t type) const;

private:
    mutable std::mutex mutex_;
    std::vector<EventSubscription> subscribers_;
    std::queue<qemu_event_t> pending_events_;
    std::vector<std::string> message_storage_;
};

} // namespace qvadis
