#pragma once

#include <condition_variable>
#include <functional>
#include <latch>
#include <mutex>
#include <queue>
#include <ranges>
#include <thread>
#include <vector>

using thread_id_t = int;
using task_t = std::function<void()>;

constexpr int MAIN_THREAD_ID = -1;
thread_local inline int thread_id = MAIN_THREAD_ID;

class ThreadPool
{
  std::vector<std::thread> m_threads;

  std::condition_variable m_queueCondition;
  std::mutex m_mutex;
  std::queue<task_t> m_tasks;
  std::queue<std::latch> m_taskLatches;
  bool m_closing = false;

public:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  ThreadPool(int const num_threads);
  ~ThreadPool();

  // empties the task queue, without finishing
  void drain();
  void block_until_finished();
  void add_job(task_t func);
};

int
run_command(std::string const command, std::span<std::string> args);

template<typename... Ts>
  requires(std::convertible_to<Ts, std::string_view> && ...)
void
run_command(std::string const command, Ts const... args)
{
  std::array<std::string, sizeof...(Ts)> arr{ std::string(args)... };
  run_command(command, arr);
}
