#include "turnstile.h"
using std::hash;
using std::mutex;
using std::queue;
using std::unique_lock;
const uint64_t INIT_Q_SZ = 16;
const uint64_t GLOBAL_MUTEX_POOL = 257;
mutex guards[GLOBAL_MUTEX_POOL];
mutex queue_guard;
struct dummy_t {
} dt;
Turnstile *dummy = reinterpret_cast<Turnstile *>(&dt);

//////////////////////////////////////
//////         MANAGER          //////
//////////////////////////////////////

Manager::Manager() {
  for (uint64_t i = 0; i < INIT_Q_SZ; i++) {
    this->turnstiles.push(new Turnstile());
  }
}
Manager::~Manager() {
  while (!turnstiles.empty()) {
    auto t = turnstiles.front();
    turnstiles.pop();
    delete t;
  }
}
Manager &manager() {
  static Manager manager;
  return manager;
}
Turnstile *Manager::popTurnstile() {
  unique_lock<mutex> lk(queue_guard);
  if (manager().turnstiles.empty()) {
    for (uint64_t i = 0; i < 2 * manager().active; i++) {
      manager().turnstiles.push(new Turnstile());
    }
  }
  Turnstile *t = manager().turnstiles.front();
  manager().turnstiles.pop();
  manager().active++;
  return t;
}
void Manager::pushTurnstile(Turnstile *t) {
  unique_lock<mutex> lk(queue_guard);
  if (manager().turnstiles.size() > INIT_Q_SZ &&
      +manager().turnstiles.size() > (3 * manager().active)) {
    uint64_t sz = manager().turnstiles.size();
    for (uint64_t i = 0; i < sz / 2; i++) {
      if (!turnstiles.empty()) {
        auto t1 = turnstiles.front();
        turnstiles.pop();
        delete t1;
      }
    }
  }
  manager().turnstiles.push(t);
}

//////////////////////////////////////
//////          MUTEX           //////
//////////////////////////////////////

Mutex::Mutex() : turnstile(nullptr) {}
void Mutex::lock() {
  auto guard_id = hash<Mutex *>{}(this) % GLOBAL_MUTEX_POOL;
  unique_lock<mutex> lk(guards[guard_id]);
  if (turnstile == nullptr) {
    turnstile = dummy;
  } else {
    if (turnstile == dummy) {
      turnstile = manager().popTurnstile();
    }
    turnstile->ctr++;
    lk.unlock();
    unique_lock<mutex> wait_lock(turnstile->m);
    turnstile->cv.wait(wait_lock, [&] { return turnstile->ready; });
    lk.lock();
    turnstile->ready = false;
    turnstile->ctr--;
    if (turnstile->ctr == 0) {
      manager().pushTurnstile(turnstile);
      turnstile = dummy;
    }
    lk.unlock();
  }
}
void Mutex::unlock() {
  auto guard_id = hash<Mutex *>{}(this) % GLOBAL_MUTEX_POOL;
  unique_lock<mutex> lk(guards[guard_id]);
  if (turnstile == dummy) {
    turnstile = nullptr;
  } else {
    unique_lock<mutex> lk1(turnstile->m);
    turnstile->ready = true;
    turnstile->cv.notify_one();
  }
  lk.unlock();
}
