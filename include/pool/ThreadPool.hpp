#pragma once
#include "Logger/Logger.hpp"

#include "Time/Timer.hpp"

#include <expected>
#include <functional>
#include <future>
#include <queue>

class Logger;

namespace pool
{
  enum class EnqueueError
  {
    PoolStopped,
  };

  struct QueuedTask
  {
    QueuedTask() = delete;

    QueuedTask(std::move_only_function<void()> t_task, const unsigned int t_taskNumber) : task(std::move(t_task)),
      taskNumber(t_taskNumber) {}

    std::move_only_function<void()> task;
    Timer                           timer{};
    unsigned int                    taskNumber;
    std::thread::id                 threadId;
  };
}

class ThreadPool
{
public:
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  explicit ThreadPool(size_t t_threadCount = 0); // 0 = auto
  ~ThreadPool();
  ThreadPool& operator=(ThreadPool& t_other)  = delete;
  ThreadPool& operator=(ThreadPool&& t_other) = delete;
  ThreadPool(ThreadPool& t_other)             = delete;
  ThreadPool(ThreadPool&& t_other) noexcept   = delete;
  //-------------------------------------------------------------------------------------------------------------------

  template <typename F>
  void AddThread(F&& t_f);

  template <typename F, typename... Args>
  [[nodiscard]] std::expected<std::future<std::invoke_result_t<F, Args...>>, pool::EnqueueError> Enqueue(
    F&&       t_f,
    Args&&... t_args);

  [[nodiscard]] size_t ThreadCount() const { return m_workerPool.size(); }

private:
  /*!
   * @brief A worker intended to be dispatched on a thread that will automatically pick up tasks that are inserted into the queue and will wait if the queue is empty.
   */
  void WorkerLoop(const std::stop_token& t_st);

  std::mutex                   m_mutex; // Mutex for inserting tasks
  std::condition_variable_any  m_cv; // Cv to wait threads
  std::queue<pool::QueuedTask> m_queue; // Task queue
  std::stop_source             m_stopSource; // Stop source to signal worker threads to stop
  std::vector<std::jthread>    m_workerPool; // Container for dispatched worker threads
  size_t                       m_maxThreadsUser    = 0; // User-defined maximum number of dispatched threads
  size_t                       m_maxThreadsHw      = std::thread::hardware_concurrency(); // Hardware-defined maximum
  size_t                       m_maxPreSpawnThread = 0; // Calculated amount of threads to dispatch pre-emptively
  size_t                       m_idleThreads       = 0; // Amount of dispatched threads that are currently idle
  std::atomic<unsigned int>    m_totalTasks        = 0; // Global task counter
  Logger*                      m_logger            = &Logger::Instance();
};

#include "ThreadPool.inl"
