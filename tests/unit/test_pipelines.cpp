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

  virtual ~IntProducer() {
    stop();
  }

  void pre_start() override {
    m_pre_start_called = true;
  }

  void start() override {
    m_start_called = true;
    m_thread = std::thread([&]() { thread_body(); });
  }

  void stop() override {
    m_stop_called = true;
    if (m_thread.joinable())
      m_thread.join();
  }

  bool m_pre_start_called{false};
  bool m_start_called{false};
  bool m_stop_called{false};

private:
  void thread_body() {
    for (int i = m_i; i <= m_i + 5; ++i) {
      // Waiting for a while will guarantee the predictable order on the receiving side (consumer)
      // and it can be therefore tested.
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

  void pre_start() override {
    m_pre_start_called = true;
  }

  void start() override {
    m_start_called = true;
  }

  void stop() override {
    m_stop_called = true;
  }

  void input(int v) override {
    m_consumed_values.push_back(v);
  }

public:
  virtual ~IntConsumer() = default;

  std::vector<int> m_consumed_values;
  bool m_start_called{false};
  bool m_pre_start_called{false};
  bool m_stop_called{false};
};

class IntDoubler: public Box, public Consumer<int>, public Producer<int> {

  void pre_start() override {
    m_pre_start_called = true;
  }

  void start() override {
    m_start_called = true;
  }

  void stop() override {
    m_stop_called = true;
  }

  void input(int v) override {
      produce(2 * v);
  }

public:
  virtual ~IntDoubler() = default;

  bool m_start_called{false};
  bool m_pre_start_called{false};
  bool m_stop_called{false};
};

class VectorProducer: public Box, public Producer<std::vector<int>> {
public:
  VectorProducer(int i, std::function<void()> pipeline_stop_callback) :
    m_i(i),
    m_pipeline_stop_callback(pipeline_stop_callback)
  {}

  virtual ~VectorProducer()
  {
    stop();
  }

  void start() override {
    m_thread = std::thread([&]() { thread_body(); });
  }

  void stop() override {
    if (m_thread.joinable())
      m_thread.join();
  }

private:
  void thread_body() {
    for (auto i = 0; i < m_i; i++)
    {
      produce({i, i+1, i+2});
    }
    m_pipeline_stop_callback();
  }

  int m_i;
  std::function<void()> m_pipeline_stop_callback;
  std::thread m_thread;
};

class VectorConsumer: public Box, public Consumer<std::vector<int>> {

  void start() override {
  }

  void stop() override {
  }

  void input(std::vector<int> v) override {
    m_consumed_values.push_back(v);
  }

public:
  virtual ~VectorConsumer() = default;

  std::vector<std::vector<int>> m_consumed_values;
  bool m_start_called{false};
  bool m_pre_start_called{false};
  bool m_stop_called{false};
};

TEST_CASE("Box registration") {
  Pipeline p1;
  Pipeline p2;
  IntConsumer consumer;
  IntDoubler doubler;

  SECTION("Succesfull box registration") {
    p1.connect<int>(doubler, consumer);
    REQUIRE(p1.m_boxes.size() == 2);
    REQUIRE(doubler.m_consumers.size() == 1);
  }

  SECTION("Same box registration on more than one pipeline shall result in the exception") {
    p1.connect<int>(doubler, consumer);
    REQUIRE_THROWS_AS(p2.connect<int>(doubler, consumer), std::logic_error);
  }
}

TEST_CASE("Boxes connected in pipelines") {
  Pipeline p;

  IntProducer producer(5, [&](){p.stop();});
  IntConsumer consumer;
  IntDoubler doubler;
  IntDoubler doubler_2;

  SECTION("Three connected boxes in the pipeline and initialization method calls test.") {
    p.connect<int>(producer, consumer);
    p.connect<int>(producer, doubler);
    p.connect<int>(doubler, consumer);

    p.run_until_stopped();

    REQUIRE(producer.m_pre_start_called);
    REQUIRE(producer.m_start_called);
    REQUIRE(producer.m_stop_called);

    REQUIRE(consumer.m_pre_start_called);
    REQUIRE(consumer.m_start_called);
    REQUIRE(consumer.m_stop_called);

    REQUIRE(doubler.m_pre_start_called);
    REQUIRE(doubler.m_start_called);
    REQUIRE(doubler.m_stop_called);

    std::vector<int> expected_values{5, 10, 6, 12, 7, 14, 8, 16, 9, 18, 10, 20};
    REQUIRE(std::equal(consumer.m_consumed_values.begin(), consumer.m_consumed_values.end(), expected_values.begin()));
  }

  SECTION("Four connected boxes in the pipeline") {
    p.connect<int>(producer, consumer);
    p.connect<int>(producer, doubler);
    p.connect<int>(doubler, doubler_2);
    p.connect<int>(doubler_2, consumer);

    p.run_until_stopped();

    std::vector<int> expected_values{5, 20, 6, 24, 7, 28, 8, 32, 9, 36, 10, 40};
    REQUIRE(std::equal(consumer.m_consumed_values.begin(),
                       consumer.m_consumed_values.end(),
                       expected_values.begin()));
  }

  SECTION("Passing vector through the pipeline") {
    const auto PRODUCED_VECTORS = 500;
    VectorProducer vector_producer(PRODUCED_VECTORS , [&](){p.stop();});
    VectorConsumer vector_consumer;

    p.connect<std::vector<int>>(vector_producer, vector_consumer);

    std::vector<std::vector<int>> expected_values;
    for (auto i = 0; i < PRODUCED_VECTORS ; i++)
    {
      expected_values.push_back({i, i+1, i+2});
    }
    p.run_until_stopped();

    REQUIRE(std::equal(vector_consumer.m_consumed_values.begin(),
                       vector_consumer.m_consumed_values.end(),
                       expected_values.begin()));
  }
}
