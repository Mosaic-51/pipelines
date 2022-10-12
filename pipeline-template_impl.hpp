#pragma once

namespace mosaic::pipeline {

template <typename ValueT>
void Producer<ValueT>::produce(ValueT v) {
    std::unique_lock lock(m_box_this->m_mutex);

    if (!m_box_this)
        throw std::logic_error("Can't produce values without being associated with a pipeline");
    assert(m_box_this->m_associated_pipeline);

    if (m_consumers.empty())
        return; // Don't bother producing values if noone listens

    m_buffered.emplace_back(std::move(v));
    m_box_this->m_associated_pipeline->register_waiting_producer(this);
}

template <typename ValueT>
void Producer<ValueT>::send_buffered() {
    std::unique_lock lock(m_box_this->m_mutex);
    for (auto& buffer: m_buffered) {
        for (auto consumer: m_consumers)
            consumer->input(buffer);
    }
    m_buffered.clear();
}

template <typename ValueT>
void Producer<ValueT>::connect(Consumer<ValueT> *consumer, Box *box_this) {
    std::unique_lock lock(box_this->m_mutex);

    assert(m_box_this == nullptr || m_box_this == box_this);
    assert(dynamic_cast<Box*>(this) == box_this);

    m_box_this = box_this;
    m_consumers.push_back(consumer);
}

template <typename ValueT, typename SourceBox, typename DestinationBox>
void Pipeline::connect(SourceBox& source, DestinationBox& destination) {
    static_assert(std::is_convertible_v<SourceBox*, Box*>, "Source must inherit from Box");
    static_assert(std::is_convertible_v<SourceBox*, Producer<ValueT>*>, "Source must inherit from Producer<ValueT>");
    static_assert(std::is_convertible_v<DestinationBox*, Box*>, "Destination must inherit from Box");
    static_assert(std::is_convertible_v<DestinationBox*, Consumer<ValueT>*>, "Destination must inherit from Consumer<ValueT>");

    std::unique_lock lock(m_mutex);
    register_box(source);
    register_box(destination);
    source.connect(&destination, &source);
}

}
