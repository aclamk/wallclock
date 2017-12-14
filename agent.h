/*
 * agent.h
 *
 *  Created on: Dec 1, 2017
 *      Author: adam
 */

#ifndef AGENT_H_
#define AGENT_H_

#include <vector>
#include <atomic>
#include <semaphore.h>
struct conveyor;
struct callstep;

class thread_sampling_ctx
{
  thread_sampling_ctx(size_t depth);

public:
  conveyor* conv{nullptr};
  callstep* root{nullptr};
  std::atomic<uint64_t> counter{0};
  std::atomic<uint64_t> pfunc{0};
  std::atomic<bool> lock{0};
  /*creates sampling context for calling thread*/
  static thread_sampling_ctx* create();
  void peek();
};


class agent
{
  agent();
public:
  static agent* create();
  void add_thread(thread_sampling_ctx* sc);

//private:
  std::vector<thread_sampling_ctx*> threads;
  sem_t wake_up;
  enum {
    CMD_TRACE_THREAD_NEW=1,
    CMD_TRACE_THREAD_END=2,
    CMD_TERMINATE=3
  };

  static int worker(void*);
private:
  int wait_read();
  bool read_bytes(void* ptr, size_t size);
  bool write_bytes(const void* ptr, size_t size);

  int conn_fd;
  bool worker();

  int read_command();

  bool trace_thread_new();

  friend int backtrace_reader(void* arg);
};



#endif /* AGENT_H_ */
