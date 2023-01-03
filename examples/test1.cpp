#include "../pipeline.hpp"

#include <thread>
#include <iostream>

using namespace mosaic::pipeline;

class IntProducer: public Box, public Producer<int> {
public:
    IntProducer(int i) : m_i(i) {}

    void start() override {
        std::cout << "Starting int producer\n";
        m_thread = std::thread([&]() { thread_body(); });
    }

    void stop() override {
        std::cout << "Stopping int producer\n";
        m_thread.join();
    }

private:
    void thread_body() {
        for (int i = m_i; i <= m_i + 5; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds{1});
            std::cout << "Producing " << i << std::endl;
            produce(i);
        }
    }

    int m_i;
    std::thread m_thread;
};

class IntConsumer: public Box, public Consumer<int> {
    void start() override {
        std::cout << "Starting int consumer\n";
    }

    void stop() override {
        std::cout << "Stopping int consumer\n";
    }

    void input(int v) override {
        std::cout << "Consumed number " << v << "\n";
    }
};

class IntDoubler: public Box, public Consumer<int>, public Producer<int> {
    void start() override {
        std::cout << "Starting int doubler\n";
    }

    void stop() override {
        std::cout << "Starting int doubler\n";
    }

    void input(int v) override {
        std::cout << "IntDoubler::input\n";
        produce(2 * v);
    }
};

int main() {
    Pipeline p;

    IntProducer producer(5);
    IntConsumer consumer;
    IntDoubler doubler;

    p.connect<int>(producer, consumer);
    p.connect<int>(producer, doubler);
    p.connect<int>(doubler, consumer);

    p.pre_start_associated_boxes();
    p.start_associated_boxes();
    p.run_until_stopped();

    return 0;
}
