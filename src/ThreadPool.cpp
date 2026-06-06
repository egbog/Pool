#include "pool/ThreadPool.hpp"

ThreadPool::ThreadPool(const size_t t_threadCount) : m_maxThreadsUser(t_threadCount) {
  // if we are not able to get the amount of max concurrent threads
  if (m_maxThreadsHw == 0) {
    m_logger->Log<Logger::Warning>("Unable to determine hardware concurrency, defaulting to a single worker.");
    // only allow 1 worker
    m_maxThreadsHw = 1;
  }

  // default to max hardware threads if user did not specify a thread count
  if (m_maxThreadsUser == 0) {
    m_maxThreadsUser = m_maxThreadsHw;
    m_logger->Log<Logger::Info>(std::format("No thread count specified, defaulting to hardware concurrency: {}", m_maxThreadsHw));
  }

  // make sure user did not request more threads than hw is capable of
  m_maxThreadsUser = std::min(m_maxThreadsUser, m_maxThreadsHw);

  // half of total cores, at least 1
  const size_t safeMinimumThreads = std::max<size_t>(1, m_maxThreadsUser / 2);

  // pre-spawn a few threads that can be picked up by new tasks before creating more
  // only spawn as many threads as the cpu has, if its a double core, only spawn one
  m_maxPreSpawnThread = std::min(m_maxThreadsUser, safeMinimumThreads);

  try {
    for (size_t i = 0; i < m_maxPreSpawnThread; ++i) {
      std::scoped_lock lock(m_mutex);
      AddThread([this, t_st = m_stopSource.get_token()] { WorkerLoop(t_st); });
      m_idleThreads++; // pre-spawned threads are idle until they pick up a task
    }
  }
  catch (const std::system_error& e) {
    if (m_workerPool.empty()) {
      throw; // couldn't get even one worker -> no pool, fail
    }

    m_logger->Log<Logger::Warning>(
      std::format("Spawned only {} of {} workers: {}", m_workerPool.size(), m_maxPreSpawnThread, e.what()));

    m_maxThreadsUser    = m_workerPool.size(); // cap growth to what actually succeeded
    m_maxPreSpawnThread = m_maxThreadsUser;
  }
}

ThreadPool::~ThreadPool() {
  m_stopSource.request_stop(); // single signal; the cv stop-aware wait wakes workers
  m_workerPool.clear();   // ~jthread joins each worker now, before any member dies

  const std::string msg = std::format("Thread Pool closed after accepting {} tasks.", static_cast<unsigned int>(m_totalTasks));
  m_logger->Log<Logger::Debug>(msg);
}

void ThreadPool::WorkerLoop(const std::stop_token& t_st) {
  while (true) {
    // we made this std::optional to avoid the overhead of default constructing a QueuedTask
    std::optional<pool::QueuedTask> job;

    {
      std::unique_lock lock(m_mutex);
      // make the thread wait until shutdown, or we insert a task
      m_cv.wait(lock, t_st, [this] { return !m_queue.empty(); });

      if (m_queue.empty()) {
        break;
      }

      m_idleThreads--; // thread is waking up

      // move the next element in the queue to a temp var to run
      job = std::move(m_queue.front());
      m_queue.pop();
    }

    // Measure how long this job waited in the queue
    const auto waitTime = job->timer.Elapsed();
    // assign threadId once the task gets picked up
    job->threadId = std::this_thread::get_id();

    std::string log;

    if (job->taskNumber > m_maxPreSpawnThread && job->taskNumber <= m_maxThreadsUser) {
      log = std::format("Task #{} waited {:L} before starting on new thread: {}", job->taskNumber, waitTime, job->threadId);
    }
    else if (job->taskNumber > m_maxPreSpawnThread) {
      log = std::format("Task #{} waited {:L} in queue before starting on thread: {}", job->taskNumber, waitTime, job->threadId);
    }
    else {
      log = std::format("Task #{} assigned to already running thread: {}", job->taskNumber, job->threadId);
    }
    
    m_logger->Log<Logger::Debug>(log);

    job->task(); // run job

    m_idleThreads++; // thread is idle again after finishing task
  }
}
