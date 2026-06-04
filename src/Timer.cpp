#include "pool/Time/Timer.hpp"

Timer::Timer() {
  Reset();
}

void Timer::Reset() {
  m_start = Clock::now();
}

[[nodiscard]] std::chrono::duration<double, std::milli> Timer::Elapsed() const {
  return Clock::now() - m_start;
}
