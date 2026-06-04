#pragma once
#include <chrono>

class Timer
{
public:
  using Clock     = std::chrono::high_resolution_clock;
  using TimePoint = Clock::time_point;

  Timer();
  // Reset the timer to "now"
  void                                                    Reset();
  [[nodiscard]] std::chrono::duration<double, std::milli> Elapsed() const;

private:
  TimePoint m_start;
};
