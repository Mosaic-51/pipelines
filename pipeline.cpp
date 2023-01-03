#include "pipeline.hpp"

namespace mosaic::pipeline {

bool Box::maybe_associate_with(Pipeline *pipeline) {
    std::unique_lock lock(m_mutex);

    bool differs = m_associated_pipeline != pipeline;

    if (m_associated_pipeline && differs)
        throw std::logic_error("Box is already associated with different pipeline");

    m_associated_pipeline = pipeline;
    return differs;
}

void Pipeline::run_until_stopped() {
    m_queue_waiting_producers.start();
    while (!m_stop_flag) {
        std::unique_lock lock(m_mutex);
        if (m_registered_producer.has_value())
        {
          (*m_registered_producer)->send_buffered();
        }

        m_registered_producer.reset();

        m_cond.wait(lock);
    }
}

void Pipeline::stop() {
    m_stop_flag = true;
    m_cond.notify_all();
}

void Pipeline::register_waiting_producer(detail::TypeErasedProducer *producer) {
    std::unique_lock lock(m_mutex);
    m_registered_producer.emplace(producer);
    m_cond.notify_all();
}

void Pipeline::register_box(Box &box) {
    if (box.maybe_associate_with(this)) {
        m_boxes.push_back(&box);
    }
}

void Pipeline::pre_start_associated_boxes()
{
  for (auto* box: m_boxes) {
    box->pre_start();
  }
}

void Pipeline::start_associated_boxes()
{
  for (auto* box: m_boxes) {
    box->start();
  }
}

void Pipeline::stop_associated_boxes()
{
  for (auto* box: m_boxes) {
    box->stop();
  }
}

}
