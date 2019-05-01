#ifndef SRC_TURNSTILE_H_
#define SRC_TURNSTILE_H_

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <type_traits>

class Turnstile {
 public:
  Turnstile() = default;

  ~Turnstile() = default;

  std::condition_variable cv;

  bool ready = false;

  std::mutex m;

  uint64_t ctr = 0;
};

class Manager {
 public:
  Manager();

  ~Manager();

  Turnstile *popTurnstile();

  void pushTurnstile(Turnstile *t);

  std::queue<Turnstile *> turnstiles;

  uint64_t active = 0;
};

class Mutex {
 public:
  Mutex();

  ~Mutex() = default;

  Mutex(const Mutex &) = delete;

  void lock();  // NOLINT

  void unlock();  // NOLINT

 private:
  Turnstile *turnstile;
};

#endif  // SRC_TURNSTILE_H_
