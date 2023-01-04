#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <iostream>
#include <functional>
#include <algorithm>

#include "../../pipeline.hpp"


using namespace mosaic::pipeline;

class IntProducer: public Box, public Producer<int> {
public:
  IntProducer(int i, std::function<void()> pipeline_stop_callback) :
     m_i(i),
     m_pipeline_stop_callback(pipeline_stop_callback)
  {}

  void start() override {
      m_thread = std::thread([&]() { thread_body(); });
  }

  void stop() override {
      m_thread.join();
  }

private:
  void thread_body() {
      for (int i = m_i; i <= m_i + 5; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds{50});
          produce(i);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{50});
      m_pipeline_stop_callback();
  }

  int m_i;
  std::function<void()> m_pipeline_stop_callback;
  std::thread m_thread;
};

class IntConsumer: public Box, public Consumer<int> {
  void start() override {
  }

  void stop() override {
  }

  void input(int v) override {
      m_consumed_values.push_back(v);
  }

  public:
    std::vector<int> m_consumed_values;
};

class IntDoubler: public Box, public Consumer<int>, public Producer<int> {
  void start() override {
  }

  void stop() override {
  }

  void input(int v) override {
      produce(2 * v);
  }
};

TEST_CASE( "Basic pipelines - happy path" ) {
  Pipeline p;

  IntProducer producer(5, [&](){p.stop();});
  IntConsumer consumer;
  IntDoubler doubler;

  p.connect<int>(producer, consumer);
  p.connect<int>(producer, doubler);
  p.connect<int>(doubler, consumer);

  p.run_until_stopped();
  std::cout << "pipeline finished\n";

  std::vector<int> expected_values{5, 10, 6, 12, 7, 14, 8, 16, 9, 18, 10, 20};
  REQUIRE(std::equal(consumer.m_consumed_values.begin(), consumer.m_consumed_values.end(), expected_values.begin()));
}
