#pragma once

#include <condition_variable>
#include <thread>
#include <mutex>
#include <functional>
#include <deque>

//namespace mosaic::common {

/// This class template is intended to be used for
/// the processing of exteranally queued data of type T by the user provided callback
/// in the constructor.
template<typename T>
class QueueWorker
{
public:
QueueWorker(std::function<void(T&&)> callback)
: m_callback(callback)
{
}

~QueueWorker()
{
  stop();
}

void submit(T&& item)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  m_task_queue.push_back(std::move(item));
  m_condition.notify_one();
}

void start()
{
  auto pred = [&]() { return !m_task_queue.empty() || m_quit;};

  m_thread = std::thread([&, pred = pred]() {
    while (true)
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_condition.wait(lock, pred);
      if (m_quit)
      {
        return;
      }
      T item = std::move(m_task_queue.front());
      m_task_queue.pop_front();
      lock.unlock();
      m_callback(std::move(item));
    }
  });
}

void stop()
{
  m_quit = true;
  m_condition.notify_one();
  m_thread.join();
}

private:
std::function<void(T&&)> m_callback;
std::deque<T> m_task_queue;
std::mutex m_mutex;
std::condition_variable m_condition;
std::atomic_bool m_quit{false};
std::thread m_thread;

};

//} // namespace mosaic::common
