#include "common.hh"
#include "thread_pool.hh"
#include <sys/wait.h>

// thread_local int thread_id = MAIN_THREAD_ID;

ThreadPool::ThreadPool(int const num_threads)
{
  // wrap the entire thing here in
  // a try catch, such that
  // these also do not bubble up
  // to the main thread
  auto const thread_dispatch = [&](thread_id_t const this_thread_id) {
    try {
      task_t task = nullptr;
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

          task = m_tasks.front();
          m_tasks.pop();
        }

        m_workingThreads += 1;

        task();

        // fetch_sub is fetch-set,
        // so if it returns 1 its now 0
        if (m_workingThreads.fetch_sub(1) == 1)
          m_finishedCondition.notify_one();
      }
    } catch (std::exception const&) {
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

void
ThreadPool::block_until_finished()
{
  // this is all in a loop,
  // and the convar is wait_for because theres
  // a chance that a deadlock can happen
  // if a thread pushes in new work and finished between
  // the first m_tasks empty check
  // and the condition variable wait_for
  for (;;) {
    // if the task queue is already empty,
    // just quit
    {
      std::scoped_lock lock(m_mutex);
      if (m_tasks.empty())
        return;
    }

    std::unique_lock lock(m_mutex);

    // wait until finishedCondition gets triggered
    // and tasks is empty
    m_finishedCondition.wait_for(
      lock, std::chrono::seconds(1), [this] { return m_tasks.empty(); });
  }
}

void
ThreadPool::add_job(task_t func)
{
  {
    std::scoped_lock lock(m_mutex);
    m_tasks.push(func);
  }

  m_queueCondition.notify_one();
  // threadsafe_print("added task\n");
}

int
run_command(std::string const command, std::span<std::string> args)
{

  // use pipes to redirect stdout
  int fds[2];

  pipe(fds);

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

    threadsafe_print(what, "\n");
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

    return child_exit_code;
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
