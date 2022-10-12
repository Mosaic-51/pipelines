#pragma once

#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <vector>
#include <cassert>

namespace mosaic::pipeline {

class Pipeline;

namespace detail {

/// Allows for sending buffered messages regardles of message type
class TypeErasedProducer {
public:
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
    Box(Box&&) = delete; // Boxes are referenced by pipeline, can't be moved (TODO)
    Box(const Box&): Box() {} // Copied box is not associated with any pipeline
    void operator=(const Box&) = delete;
    void operator=(Box&&) = delete;
    virtual ~Box() {}

    /// Called when starting the pipeline.
    /// At this time all producers connected to this box have been pre-started.
    /// May be used ask producers for mock data to prime internal buffers.
    /// Called from the main thread
    /// TODO: In fact this is never called now!
    virtual void pre_start() {};

    /// Called when starting the pipeline.
    /// At this time all boxes have been pre-started and all consumers
    /// connected to this box have been started
    /// After this call the box may start producing data.
    /// Called from the main thread
    /// TODO: In fact this is never called now!
    virtual void start() {};

    /// Called when stopping the pipeline.
    /// At this time all producers connected to this box have been stopped.
    /// After this call the box must not produce data any more.
    /// Called from the main thread
    /// TODO: In fact this is never called now!
    virtual void stop() {};

private:
    /// Associated this box with the pipeline, unless already associated.
    /// Returns true if the box was not associated with pipeline before.
    /// Throws exception if already associated with different pipeline.
    bool maybe_associate_with(Pipeline *pipeline);

    std::mutex m_mutex;
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
    Consumer(const Consumer&): Consumer() {} // Copying consumers is fine
    Consumer(Consumer&&) = delete; // Consumers are referenced in producers, can't be moved (TODO)
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
/// TODO: Maybe we want to support producers that buffer internally, without extra std::vector?
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

    Box *m_box_this = nullptr;
        // Pointer to the box that uses this mixin (side-cast to box)
        // This value gets set when connecting and is used to access the shared functionality of the box

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

    void run_until_stopped();
    void stop();

    /// Connect output of source to destination
    template <typename ValueT, typename SourceBox, typename DestinationBox>
    void connect(SourceBox& source, DestinationBox& destination);

private:
    void register_waiting_producer(detail::TypeErasedProducer *producer);
    void register_box(Box &box);

    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::vector<detail::TypeErasedProducer*> m_waiting_producers;
    bool m_stop_flag = false;
    std::vector<Box *> m_boxes;

    template <typename T>
    friend class Producer;
};

}

#include "pipeline-template_impl.hpp"
