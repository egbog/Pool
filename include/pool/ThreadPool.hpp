#pragma once
#include "Logger/Logger.hpp"

#include "Time/Timer.hpp"

#include <future>
#include <queue>

// TODO: shutdown function?

class Logger;

namespace obj
{
  struct QueuedTask
  {
    QueuedTask() = delete;

    QueuedTask(std::packaged_task<void()> t_task, const unsigned int t_taskNumber) : task(std::move(t_task)), timer(Timer()),
                                                                                     taskNumber(t_taskNumber) {}

    [[nodiscard]] static std::string ThreadIdString(const std::thread::id& t_id);

    std::packaged_task<void()> task;
    Timer                      timer;
    unsigned int               taskNumber;
    std::thread::id            threadId;
  };
}

class ThreadPool
{
public:
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  explicit ThreadPool(size_t t_threadCount);
  ~ThreadPool();
  ThreadPool& operator=(ThreadPool& t_other)  = delete;
  ThreadPool& operator=(ThreadPool&& t_other) = delete;
  ThreadPool(ThreadPool& t_other)             = delete;
  ThreadPool(ThreadPool&& t_other) noexcept   = delete;
  //-------------------------------------------------------------------------------------------------------------------

  template <typename F>
  void AddThread(F&& t_f);

  template <typename F, typename... Args>
  std::future<std::invoke_result_t<F, Args...>> Enqueue(F&& t_f, Args&&... t_args);

  [[nodiscard]] constexpr size_t ThreadCount() const { return m_workerPool.size(); }

private:
  /*!
   * @brief A worker intended to be dispatched on a thread that will automatically pick up tasks that are inserted into the queue and will wait if the queue is empty.
   */
  void WorkerLoop();

  std::mutex                  m_mutex; // Mutex for inserting tasks
  std::condition_variable     m_cv; // Cv to wait threads
  std::queue<obj::QueuedTask> m_queue; // Task queue
  std::vector<std::jthread>   m_workerPool; // Container for dispatched worker threads
  size_t                      m_maxThreadsUser    = 0; // User-defined maximum number of dispatched threads
  size_t                      m_maxThreadsHw      = std::thread::hardware_concurrency(); // Hardware-defined maximum
  size_t                      m_maxPreSpawnThread = 0; // Calculated amount of threads to dispatch pre-emptively
  size_t                      m_idleThreads       = 0; // Amount of dispatched threads that are currently idle
  bool                        m_shutdown          = false;
  bool                        m_poolActive        = false;
  std::atomic<unsigned int>   m_totalTasks        = 0; // Global task counter
  Logger*                     m_logger            = &Logger::Instance();
};

#include "ThreadPool.inl"
