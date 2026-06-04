#pragma once

#define NOMINMAX
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <queue>
#include <windows.h>

class Logger
{
public:
  enum LogSeverity : uint8_t
  {
    Error,
    Warning,
    Info,
    Debug,
    None
  };

  struct LogEntry
  {
    LogEntry(std::string t_message, const LogSeverity t_severity) : message(std::move(t_message)), severity(t_severity) {}
    std::string message;
    LogSeverity severity;
  };

  struct ConsoleColor
  {
    HANDLE h;
    WORD   original;

    ConsoleColor(const HANDLE t_h, const WORD t_c) : h(t_h) {
      CONSOLE_SCREEN_BUFFER_INFO info;
      GetConsoleScreenBufferInfo(t_h, &info);
      original = info.wAttributes;
      SetConsoleTextAttribute(t_h, t_c);
    }

    ~ConsoleColor() { SetConsoleTextAttribute(h, original); }
    ConsoleColor& operator=(const ConsoleColor&) = delete;
    ConsoleColor& operator=(ConsoleColor&&)      = delete;
    ConsoleColor(const ConsoleColor&)            = delete;
    ConsoleColor(ConsoleColor&&)                 = delete;
  };

  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  ~Logger();
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;
  //-------------------------------------------------------------------------------------------------------------------

  static Logger& Instance() {
    static Logger instance;
    return instance;
  }

  void DispatchWorkerThread();

  template <LogSeverity Severity>
  void Log(const std::string& t_entry) {
    ThreadSafeLogMessage(LogEntry(t_entry, Severity));
  }

  void Shutdown();

  LogSeverity           currentLogLevel     = Debug; // The severity level of log messages to print
  LogSeverity           currentDiskLogLevel = Debug; // The severity level of log messages to print
  std::filesystem::path pathToLog = "logs/";
  std::string           logName = "log.txt";

private:
  Logger() = default;
  [[nodiscard]] bool    IsLogLevelEnabled(LogSeverity t_logLevel, bool t_disk = false) const;
  constexpr static WORD GetSeverityColor(LogSeverity t_logLevel);
  void                  ThreadSafeLogMessage(LogEntry t_entry);
  void                  WorkerThread();
  void                  FlushQueue();

  std::jthread                       m_thread;         // Worker thread
  std::queue<LogEntry>               m_logQueue;       // The message queue
  std::mutex                         m_waitLogMutex;   // Mutex for inserting and popping into the queue
  std::condition_variable            m_cv;             // Cv to wait thread
  std::thread::id                    m_workerThreadId; // The thread id of the dispatched worker
  bool                               m_shutdown  = false;
  bool                               m_logToDisk = false;
  std::ofstream                      m_diskFile; // disk log file
  std::map<LogSeverity, std::string> m_severityNames = {{Error, "Error"}, {Warning, "Warning"}, {Info, "Info"}, {Debug, "Debug"}};
};
