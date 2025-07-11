#include <mutex>
#include <ranges>
#include <sys/wait.h>
#include <vector>

#include "common.hh"
#include "thread_pool.hh"

// thread_local int thread_id = MAIN_THREAD_ID;

ThreadPool::ThreadPool(int const num_threads)
{
  // wrap the entire thing here in
  // a try catch, such that
  // these also do not bubble up
  // to the main thread
  auto const thread_dispatch = [&](thread_id_t const this_thread_id) {
    try {
      std::unique_ptr<TaskBase> task;
      thread_id = this_thread_id;

      for (;;) {
        {
          std::unique_lock lock(m_mutex);

          // can only accept if the job queue has something
          // or we're shutting down
          m_queueCondition.wait(lock,
                                [&] { return !m_tasks.empty() or m_closing; });

          if (m_closing)
            break;

          task = std::move(m_tasks.front());
          m_tasks.pop();
        }

        (*task.get())();
      }
    } catch (std::exception const& e) {
      threadsafe_print(e.what(), '\n');
      return;
    }
  };

  for (auto const thread_id : std::ranges::iota_view(0, num_threads)) {
    m_threads.emplace_back(std::thread(thread_dispatch, thread_id));
  }
}

ThreadPool::~ThreadPool()
{
  m_closing = true;
  m_queueCondition.notify_all();

  for (auto& th : m_threads)
    th.join();
}

void
ThreadPool::drain()
{
  std::scoped_lock lock(m_mutex);
  while (not m_tasks.empty())
    m_tasks.pop();
}

// void
// ThreadPool::block_until_finished()
// {
//   auto get_next_latch = [&]() -> std::latch& {
//     std::scoped_lock lock(this->m_mutex);
//     return this->m_taskLatches.front();
//   };

//   auto pop_latch = [&]() -> bool {
//     std::scoped_lock lock(this->m_mutex);
//     this->m_taskLatches.pop();
//     return this->m_taskLatches.empty();
//   };

//   {
//     std::scoped_lock lock(this->m_mutex);
//     if (this->m_taskLatches.empty())
//       return;
//   }

//   for (;;) {
//     auto& latch = get_next_latch();
//     latch.wait();
//     if (pop_latch())
//       break;
//   }
// }

std::pair<int, std::string>
run_command(std::string const command, std::span<std::string const> args)
{
  // use pipes to redirect stdout
  int fds[2];

  if (pipe(fds) != 0)
    throw std::runtime_error("error in pipe()");

  std::vector<std::string> args_owned(args.begin(), args.end());
  std::vector<char const*> args_owned_ptrs;
  args_owned_ptrs.push_back(command.c_str());
  for (auto const& str : args_owned)
    args_owned_ptrs.push_back(str.c_str());
  args_owned_ptrs.push_back(nullptr);

  // print what command we're executing
  // we do it here because all of the output from
  // subcommands are deferred & buffered until execution is finished
  {
    std::string what(command);
    what += " ";

    for (auto const& arg : args) {
      what.append(arg);
      what.append(" ");
    }

    threadsafe_print_verbose(what, "\n");
  }

  auto const pid = fork();

  if (pid == -1)
    throw std::runtime_error("fork failed");

  if (pid != 0) {
    char buf[512];
    int written = 0;

    // have to close this stdout first
    // or else read doesn't hit EOF... for some reason...
    close(fds[1]);

    // also includes stderr
    std::string stdout_buf;

    while (true) {
      written = read(fds[0], buf, 512);

      if (written == -1)
        throw std::runtime_error(
          "error when trying to read fd to redirect stdout in child process");

      // eof, break
      if (written == 0)
        break;

      stdout_buf.append(buf, written);

      if (stdout_buf.size() > 5_mb)
        throw std::runtime_error(
          "output of executed command exceeded 5mb of output");
    }

    close(fds[0]);

    threadsafe_print(stdout_buf);

    int childstatus = 0;
    waitpid(pid, &childstatus, 0);

    if (not WIFEXITED(childstatus))
      throw std::runtime_error("child command failed to exit normally");

    int const child_exit_code = WEXITSTATUS(childstatus);

    return { child_exit_code, stdout_buf };
  }

  // in an entirely new process here,
  // don't have to worry about async io
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[1], STDERR_FILENO);
  close(fds[0]);
  close(fds[1]);

  // actually run the command
  auto const e = execvp(command.c_str(), (char* const*)args_owned_ptrs.data());

  if (e == -1)
    throw std::runtime_error("unable to run command");

  exit(0);
}
