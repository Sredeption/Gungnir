#include "Transport.h"

namespace Gungnir {

void Transport::RpcNotifier::completed() {

}

void Transport::RpcNotifier::failed() {

}

void intrusive_ptr_add_ref(Transport::Session *session) {
    session->refCount.fetch_add(1, std::memory_order_relaxed);
}

void intrusive_ptr_release(Transport::Session *session) {
    if (session->refCount.fetch_sub(1, std::memory_order_release) == 1) {
        std::atomic_thread_fence(std::memory_order_acquire);
        session->release();
    }
}

}
