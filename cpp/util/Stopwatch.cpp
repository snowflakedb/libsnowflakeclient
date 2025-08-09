#include "snowflake/Stopwatch.h"
#include <chrono>

void stopwatch_start(Stopwatch* s)
{
  if (s != NULL)
  {
    s->isStarted = true;
    s->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  }
}

void stopwatch_stop(Stopwatch* s)
{
  if (s != NULL && s->isStarted)
  {
    s->isStarted = false;
    s->elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count() - s->startTime;
  }
}

void stopwatch_reset(Stopwatch* s)
{
  if (s != NULL)
  {
    s->isStarted = false;
    s->startTime = 0;
    s->elapsedTime = 0;
  }
}

void stopwatch_restart(Stopwatch* s)
{
  if (s != NULL)
  {
    s->isStarted = true;
    s->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
    s->elapsedTime = 0;
  }
}

long stopwatch_elapsedMillis(Stopwatch* s)
{
  if (s == NULL)
  {
    return 0;
  }
  if (s->isStarted)
  {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count() - s->startTime;
  }
  return s->elapsedTime;
}

bool stopwatch_isStarted(Stopwatch* s)
{
  if (s == NULL)
  {
    return false;
  }
  return s->isStarted;
}
