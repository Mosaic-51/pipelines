#include "pipeline.hpp"

namespace mosaic::pipeline {

bool Box::maybe_associate_with(Pipeline *pipeline) {

  bool differs = m_associated_pipeline != pipeline;

  if (m_associated_pipeline && differs)
    throw std::logic_error("Box is already associated with different pipeline");

  m_associated_pipeline = pipeline;
  return differs;
}

void Pipeline::run_until_stopped() {
  pre_start_associated_boxes();
  start_associated_boxes();

  try {
    while (!m_stop_flag) {
      std::unique_lock lock(m_mutex);
      if (!m_waiting_producers.empty()) {
        auto waiting_producer = m_waiting_producers.back();
        m_waiting_producers.pop_back();
        waiting_producer->send_buffered();
      }
      else {
        // Wait only in case that there are no more waiting producers -
        // condition variable then cannot be notified before waiting
        m_cond.wait(lock);
      }
    }
  }
  catch(...)
  {
    stop_associated_boxes();
    throw;
  }

  stop_associated_boxes();
}

void Pipeline::stop() {
  m_stop_flag = true;
  m_cond.notify_all();
}

void Pipeline::register_waiting_producer(detail::TypeErasedProducer *producer) {
  // The m_mutex is locked before calling of this method from Producer::produce
  m_waiting_producers.push_back(producer);
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
