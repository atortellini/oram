#ifndef TIMER_HELPERS_H
#define TIMER_HELPERS_H

#include <stdbool.h>
#include <time.h>

#define TIME_FUNC(elapsed_time_ptr, func_call, func_output)                    \
  do {                                                                         \
    struct timespec _start_time, _end_time;                                    \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_start_time);                          \
    func_output = (func_call);                                                 \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_end_time);                            \
    (elapsed_time_ptr)->tv_sec = _end_time.tv_sec - _start_time.tv_sec;        \
    (elapsed_time_ptr)->tv_nsec = _end_time.tv_nsec - _start_time.tv_nsec;     \
    const bool _endedOnEarlierNanoSecond = (elapsed_time_ptr)->tv_nsec < 0;    \
    if (_endedOnEarlierNanoSecond) {                                           \
      (elapsed_time_ptr)->tv_sec--;                                            \
      (elapsed_time_ptr)->tv_nsec += 1000000000L;                              \
    }                                                                          \
  } while (0)

#define TIME_VOID_FUNC(elapsed_time_ptr, func_call)                            \
  do {                                                                         \
    struct timespec _start_time, _end_time;                                    \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_start_time);                          \
    (func_call);                                                               \
    clock_gettime(CLOCK_MONOTONIC_RAW, &_end_time);                            \
    (elapsed_time_ptr)->tv_sec = _end_time.tv_sec - _start_time.tv_sec;        \
    (elapsed_time_ptr)->tv_nsec = _end_time.tv_nsec - _start_time.tv_nsec;     \
    const bool _endedOnEarlierNanoSecond = (elapsed_time_ptr)->tv_nsec < 0;    \
    if (_endedOnEarlierNanoSecond) {                                           \
      (elapsed_time_ptr)->tv_sec--;                                            \
      (elapsed_time_ptr)->tv_nsec += 1000000000L;                              \
    }                                                                          \
  } while (0)

#endif
