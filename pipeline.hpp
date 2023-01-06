#pragma once

#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <vector>
#include <cassert>
#include <atomic>
#include <optional>

namespace mosaic::pipeline {

class Pipeline;

namespace detail {

/// Allows for sending buffered messages regardles of message type
class TypeErasedProducer {
public:
  virtual ~TypeErasedProducer() = default;
  /// Send all produced messages that are buffered in this producer.
  /// Always called from the main thread (Pipeline::run_until_stopped()).
  /// Must not block.
  virtual void send_buffered() = 0;
};

}

/// Base class for boxes that can be connected in a pipeline.
/// Box subclasses also need to inherit from at least one Producer<T> or Consumer<T>
/// to be able to connect into a pipeline.
class Box {
protected:
  Box() = default;
  Box(Box&&) = delete; // Boxes are referenced by pipeline, can't be moved
  Box(const Box&): Box() {} // Copied box is not associated with any pipeline
  void operator=(const Box&) = delete;
  void operator=(Box&&) = delete;
  virtual ~Box() {}

  /// May be used ask producers for mock data to prime internal buffers.
  /// Called iteratively from the pipeline main thread.
  /// The order of calls in the collection depends on the order of the
  /// box registration.
  virtual void pre_start() {};

  /// After this call the box may start producing data.
  /// Called iteratively from the pipeline main thread
  /// The order of calls in the collection depends on the order of the
  /// box registration.
  virtual void start() {};

  /// After this call the box must not produce data any more.
  /// Called iteratively from the pipeline main thread
  /// The order of calls in the collection depends on the order of the
  /// box registration.
  virtual void stop() {};

private:
  /// Associated this box with the pipeline, unless already associated.
  /// Returns true if the box was not associated with pipeline before.
  /// Throws exception if already associated with different pipeline.
  bool maybe_associate_with(Pipeline *pipeline);

  Pipeline *m_associated_pipeline = nullptr;

  template <typename T>
  friend class Producer;
  friend class Pipeline;
};

/// Mix-in that allows receiving values of type ValueT.
/// Final users need to inherit also from `Box`.
template <typename ValueT>
class Consumer {
public:
  using InputValueT = ValueT;

  Consumer() = default;
  virtual ~Consumer() = default;
  Consumer(const Consumer&): Consumer() {} // Copying consumers is fine
  Consumer(Consumer&&) = delete; // Consumers are referenced in producers, can't be moved.
  Consumer &operator=(const Consumer&) = default; // Assignment of consumers should not be a problem
  Consumer &operator=(Consumer&&) = delete; // Same as move construction

protected:
  /// Callback that signals new data being ready for processing.
  /// Always called from the main thread (Pipeline::run_until_stopped()).
  /// Must not block.
  virtual void input(InputValueT buffer) = 0;

  template <typename T>
  friend class Producer;
};

/// Mix-in that allows sending valuse of type ValueT.
/// Final users need to inherit also from `Box`.
/// The ValueT is passed by value in the associated pipeline, therefore it is
/// recommended to wrap ValueT within the shared_ptr or similar in case of pushing
/// the buffers through the pipeline (it ensures the zero-copy passing)
template <typename ValueT>
class Producer: private detail::TypeErasedProducer {
public:
  using OutputValueT = ValueT;

  Producer() = default;
  Producer(Producer&&) = delete;
  Producer(const Producer&): Producer() {}
  Producer &operator=(const Producer&) = delete;
  Producer &operator=(Producer&&) = delete;

protected:
  /// Make a value available to our consumers
  void produce(OutputValueT v);

private:
  void send_buffered() override final;
  void connect(Consumer<ValueT> *consumer, Box *box_this);

  /// Pointer to the box that uses this mixin (side-cast to box)
  /// This value gets set when connecting and is used to access the shared functionality of the box
  Box *m_box_this = nullptr;

  std::vector<Consumer<OutputValueT> *> m_consumers;
  std::vector<OutputValueT> m_buffered;

  friend class Pipeline;
};

/// A Pipeline represents a graph of Boxes, where each box can produce and consume values.
///
/// Values transfered by the system need to be copyable and fast to copy.
class Pipeline {
public:
  Pipeline() = default;
  Pipeline(const Pipeline&) = delete;
  Pipeline(Pipeline&&) = delete;
  void operator=(const Pipeline&) = delete;
  void operator=(Pipeline&&) = delete;

  /// The main processing method that either pick up the waiting producer and send its data
  /// or wait for notification if all scheduler producers are processed.
  void run_until_stopped();
  /// Unblock and finish the run_until_stopped method.
  void stop();

  /// Connect output of source to destination
  template <typename ValueT, typename SourceBox, typename DestinationBox>
  void connect(SourceBox& source, DestinationBox& destination);

private:
  /// Schedule the waiting producers.
  void register_waiting_producer(detail::TypeErasedProducer *producer);
  /// Register the box. Called when connecting the boxes in the pipeline.
  void register_box(Box &box);

  /// Calls pre_start method on all registered boxes.
  /// This method is called internally from the main thread.
  void pre_start_associated_boxes();

  /// Calls start method on all registered boxes.
  /// This method is called internally from the main thread after pre_start_associated_boxes.
  void start_associated_boxes();

  /// Calls stop method on all registered boxes.
  /// This method is called in the end of the main processing thread.
  void stop_associated_boxes();

  std::recursive_mutex m_mutex;
  std::condition_variable_any m_cond;
  std::vector<detail::TypeErasedProducer*> m_waiting_producers;

  std::atomic_bool m_stop_flag = false;
  std::vector<Box*> m_boxes;

  template <typename T>
  friend class Producer;
};

}

#include "pipeline-template_impl.hpp"
