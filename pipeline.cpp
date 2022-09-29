#include "pipeline.hpp"

namespace mosaic::pipelines {

void Pipeline::run_until_stopped() {
    std::unique_lock lock(m_mutex);
    while (!m_stop_flag) {
        for (auto producer: m_waiting_producers)
            producer->send_buffered();
        m_waiting_producers.clear();

        m_cond.wait(lock);
    }
}

void Pipeline::stop() {
    std::unique_lock lock(m_mutex);
    m_stop_flag = true;
    m_cond.notify_all();
}

void Pipeline::register_waiting_producer(detail::TypeErasedProducer *producer) {
    std::unique_lock lock(m_mutex);
    m_waiting_producers.push_back(producer);
    m_cond.notify_all();
}

}
