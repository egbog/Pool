#include "pool/ThreadPool.hpp"


std::string obj::QueuedTask::ThreadIdString(const std::thread::id& t_id) {
  std::ostringstream s;
  s << t_id;
  return s.str();
}

ThreadPool::ThreadPool(const size_t t_threadCount) : m_maxThreadsUser(t_threadCount) {
  // if we are not able to get the amount of max concurrent threads
  if (m_maxThreadsUser == 0 || m_maxThreadsHw == 0) {
    // only run on the main thread
    return;
  }

  // make sure user did not request more threads than hw is capable of
  m_maxThreadsUser = std::min(m_maxThreadsUser, m_maxThreadsHw);

  // half of physical cores, at least 1
  const size_t safeMinimumThreads = std::max<size_t>(1, m_maxThreadsUser / 2);

  // pre-spawn a few threads that can be picked up by new tasks before creating more
  // only spawn as many threads as the cpu has, if its a double core, only spawn one
  m_maxPreSpawnThread = std::min(m_maxThreadsUser, safeMinimumThreads);

  for (size_t i = 0; i < m_maxPreSpawnThread; ++i) {
    AddThread([this] { WorkerLoop(); });
  }

  m_poolActive = true;
}

ThreadPool::~ThreadPool() {
  if (!m_poolActive) {
    return;
  }

  {
    std::lock_guard lock(m_mutex);
    m_shutdown = true;
  }
  m_cv.notify_all();
  // No need to manually join m_workers, jthreads will join automatically
  const std::string msg = std::format("Thread Pool closed after processing {} tasks.", static_cast<unsigned int>(m_totalTasks));
  m_logger->Log<Logger::Debug>(msg);
  m_poolActive = false;
}

void ThreadPool::WorkerLoop() {
  while (true) {
    // we made this std::optional to avoid the overhead of default constructing a QueuedTask
    std::optional<obj::QueuedTask> optTask;

    {
      std::unique_lock lock(m_mutex);
      m_idleThreads++; // thread is now idle
      // make the thread wait until shutdown, or we insert a task
      m_cv.wait(lock, [this] { return m_shutdown || !m_queue.empty(); });
      m_idleThreads--; // thread is waking up

      if (m_shutdown && m_queue.empty()) {
        break;
      }

      // move the next element in the queue to a temp var to run
      optTask = std::move(m_queue.front());
      m_queue.pop();
    }

    // Measure how long this job waited in the queue
    const auto waitTime = optTask->timer.Elapsed();
    // assign threadId once the task gets picked up
    optTask->threadId = std::this_thread::get_id();

    std::string log;

    if (optTask->taskNumber > m_maxPreSpawnThread && optTask->taskNumber <= m_maxThreadsUser) {
      log = std::format(
        "Task #{} waited {:L} before starting on new thread: {}",
        optTask->taskNumber,
        waitTime,
        obj::QueuedTask::ThreadIdString(optTask->threadId));
    }
    else if (optTask->taskNumber > m_maxPreSpawnThread) {
      log = std::format(
        "Task #{} waited {:L} in queue before starting on thread: {}",
        optTask->taskNumber,
        waitTime,
        obj::QueuedTask::ThreadIdString(optTask->threadId));
    }
    else {
      log = std::format(
        "Task #{} assigned to already running thread: {}",
        optTask->taskNumber,
        obj::QueuedTask::ThreadIdString(optTask->threadId));
    }

    if (!log.empty()) {
      m_logger->Log<Logger::Debug>(log);
    }

    optTask->task(); // run job
  }
}
