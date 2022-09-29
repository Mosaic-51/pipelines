#pragma once

#include <vector>
#include <stdexcept>
#include <mutex>

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

/// Mix-in that allows receiving buffers of type T.
/// Final users need to inherit also from `Box`.
template <typename T>
class Consumer {
public:
    using InputValueT = T;

protected:
    /// Callback that signals new data being ready for processing.
    /// Always called from the main thread (Pipeline::run_until_stopped()).
    /// Must not block.
    virtual void input(InputValueT buffer) = 0;
};

/// Mix-in that allows sending buffers of type T.
/// Final users need to inherit also from `Box`.
/// BoxT is a CRTP argument, must be subclass of Box.
/// TODO: Maybe we want to support producers that buffer internally, without extra std::vector?
template <typename BoxT, typename ValueT>
class Producer: public BoxT, private detail::TypeErasedProducer {
public:
    using OutputValueT = ValueT;

protected:
    /// Make a buffer available to our consumers
    void produce(OutputValueT&& buffer) {
        std::unique_lock lock(BoxT::m_mutex);

        if (!BoxT::m_associated_pipeline)
            throw std::exception("Can't produce values without being associated with a pipeline");
        if (m_consumers.empty())
            return; // Don't bother producing values if noone listens

        m_buffered.push_back(buffer);
        BoxT::m_associated_pipeline->register_waiting_producer();
    }

private:
    void send_buffered() override final {
        std::unique_lock lock(BoxT::m_mutex);
        for (auto& buffer: m_buffered) {
            for (auto consumer: m_consumers)
                consumer->input(buffer);
        }
        m_buffered.clear();
    }

    template <typename U>
    void connnect_consumer(Consumer<U> *consumer) {
        static_assert(
            std::is_same_v<OutputValueT, U>,
            "Only boxes with compatible types can be connected"
        );

        std::unique_lock lock(BoxT::m_mutex);
        m_consumers.push_back(consumer);
    }

    std::vector<Consumer<OutputValueT> *> m_consumers;
    std::vector<OutputValueT> m_buffered;

    friend class Pipeline;
};

/// Base class for boxes that can be connected in a pipeline.
/// Each box subclass must also inherit from at least one Consumer<T> or
/// Producer<T> mix-ins.
class Box {
protected:
    virtual void start() {};
    virtual void stop() {};

private:
    std::mutex m_mutex;
    Pipeline *m_associated_pipeline = nullptr;

    template <typename T, typename U>
    friend class Producer;
};

}
