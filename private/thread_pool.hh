#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <latch>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

using thread_id_t = int;
using task_t = std::function<void()>;

constexpr int MAIN_THREAD_ID = -1;
thread_local inline int thread_id = MAIN_THREAD_ID;

class ThreadPool
{
  struct TaskBase
  {
    TaskBase() = default;
    virtual ~TaskBase() = default;

    virtual void operator()() = 0;
  };

  template<typename T>
  struct Task : TaskBase
  {
    Task(T fn)
      : m_fn(fn)
    {
    }

    void operator()() final { m_promise.set_value(m_fn()); }

    T m_fn;
    std::promise<decltype(std::declval<T>()())> m_promise;
  };

  std::vector<std::thread> m_threads;

  std::condition_variable m_queueCondition;
  std::mutex m_mutex;
  std::queue<std::unique_ptr<TaskBase>> m_tasks;
  bool m_closing = false;

  void internal_add_job(TaskBase* base);

public:
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  ThreadPool(int const num_threads);
  ~ThreadPool();

  // empties the task queue, without finishing
  void drain();

  // responsibility of lifetime
  // for task moves into ThreadPool
  auto add_job(auto fn)
  {
    auto task = new Task(fn);
    auto future = task->m_promise.get_future();

    {
      std::scoped_lock lock(m_mutex);
      m_tasks.push(std::unique_ptr<TaskBase>(task));
      m_queueCondition.notify_one();
    }

    return std::move(future);
  }
};

// returns the exit code & stdout + stderr
std::pair<int, std::string>
run_command(std::string const command, std::span<std::string const> args);

template<typename... Ts>
  requires(std::convertible_to<Ts, std::string_view> && ...)
auto
run_command(std::string const command, Ts const... args)
{
  std::array<std::string, sizeof...(Ts)> arr{ std::string(args)... };
  return run_command(command, arr);
}
