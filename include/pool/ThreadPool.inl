#pragma once

template <typename F>
void ThreadPool::AddThread(F&& t_f) {
  m_workerPool.emplace_back(std::forward<F>(t_f));
}

template <typename F, typename... Args>
std::future<std::invoke_result_t<F, Args...>> ThreadPool::Enqueue(F&& t_f, Args&&... t_args) {
  // the return type of the function being passed
  using ReturnT = std::invoke_result_t<F, Args...>;
  // Wrap the function and its arguments into a packaged_task

  auto task = std::packaged_task<ReturnT()>(
    [f = std::forward<F>(t_f), ...args = std::forward<Args>(t_args)]() mutable
    {
      return std::invoke(std::move(f), std::move(args)...);
    });

  // get future of task with the proper return type
  auto fut = task.get_future();

  // don't allow enqueueing after stopping the pool
  if (m_shutdown) {
    m_logger->Log<Logger::Warning>("Prevented enqueue on stopped Thread Pool");
    return fut;
  }

  // run on main thread only
  if (m_maxThreadsUser == 0) {
    task();
    return fut;
  }

  {
    std::lock_guard lock(m_mutex);
    unsigned int    taskNumber = ++m_totalTasks;
    // lambda wrap to a void() task to insert into queue
    m_queue.emplace(std::packaged_task<void()>([t = std::move(task)]() mutable { t(); }), taskNumber);

    // If all threads are busy, and we haven't reached maxThreads, spawn a new one
    if (m_idleThreads == 0 && ThreadCount() < m_maxThreadsUser) {
      AddThread([this] { WorkerLoop(); });
    }
  }

  m_cv.notify_one();
  return fut;
}
