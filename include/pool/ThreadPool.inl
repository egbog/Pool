#pragma once

template <typename F>
void ThreadPool::AddThread(F&& t_f) {
  m_workerPool.emplace_back(std::forward<F>(t_f));
}

template <typename F, typename... Args>
std::expected<std::future<std::invoke_result_t<F, Args...>>, pool::EnqueueError> ThreadPool::Enqueue(F&& t_f, Args&&... t_args) {
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

  std::lock_guard lock(m_mutex);

  // don't allow enqueueing after stopping the pool
  if (m_stopSource.stop_requested()) {
    m_logger->Log<Logger::Warning>("Prevented enqueue on stopped Thread Pool");
    return std::unexpected(pool::EnqueueError::PoolStopped);
  }

  {
    unsigned int taskNumber = ++m_totalTasks;
    // lambda wrap to a void() task to insert into queue
    m_queue.emplace([t = std::move(task)]() mutable { t(); }, taskNumber);

    // If all threads are busy, and we haven't reached maxThreads, spawn a new one
    if (m_queue.size() > m_idleThreads && ThreadCount() < m_maxThreadsUser) {
      AddThread([this, t_st = m_stopSource.get_token()] { WorkerLoop(t_st); });
      m_idleThreads++; // new thread is idle until it picks up a task
    }
  }

  m_cv.notify_one();
  return fut;
}
