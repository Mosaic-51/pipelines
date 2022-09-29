#pragma once

#include <mutex>
#include <condition_variable>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "box.hpp"

namespace mosaic::pipeline {

/// A Pipeline represents a graph of Boxes, where each box can produce and consume values.
///
/// Buffers transfered by the system need to be copyable and fast to copy,
/// similar to std::shared_ptr.
class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;

    void run_until_stopped();
    void stop();

    /// Connect output of source to destination
    /// Type BufferT is used to disambiguate which output is connected in case of
    /// multiple 
    template <typename BufferT, typename SourceBox, typename DestinationBox>
    void connect(SourceBox& source, DestinationBox& destination) {
        std::unique_lock lock(m_mutex);
        source.associate_with(this);
        destination.associate_with(this);
        source.connect(destination);
    }

    /*template <typename BufferT, typename SourceBox, typename DestinationBox>
    void connect(SourceBox& source, DestinationBox& destination) {
        std::unique_lock lock(m_mutex);
        source.associate_with(this);
        destination.associate_with(this);
        source.Producer<BufferT>::connect(destination);
    }*/

private:
    void register_waiting_producer(detail::TypeErasedProducer *producer);
    void register_box(Box *box);

    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::vector<detail::TypeErasedProducer *> m_waiting_producers;
    bool m_stop_flag;
    std::unordered_set<Box *> m_boxes;
};

}
