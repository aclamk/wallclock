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

#include "unix_io.h"
struct conveyor;
struct callstep;
class Agent;

class thread_sampling_ctx
{
  thread_sampling_ctx(size_t size);

public:
  conveyor* conv{nullptr};
  callstep* root{nullptr};
  std::atomic<uint64_t> counter{0};
  std::atomic<uint64_t> pfunc{0};
  std::atomic<bool> lock{0};
  /*creates sampling context for calling thread*/
  static thread_sampling_ctx* create();
  void peek();
  bool dump_tree(UnixIO& io);
};


class Agent
{
  Agent();
public:
  static Agent* create();
  void add_thread(thread_sampling_ctx* sc);

//private:
  std::vector<thread_sampling_ctx*> threads;
  sem_t wake_up;
  enum {
    CMD_TRACE_THREAD_NEW=1,
    CMD_TRACE_THREAD_END=2,
    CMD_TERMINATE=3,
    CMD_DUMP_TREE=4
  };

  static int worker(void*);
  UnixIO io;
private:

  int conn_fd;
  bool worker();

  int read_command();

  bool trace_thread_new();
  bool dump_tree(thread_sampling_ctx* tsx);
  bool dump_tree();
  friend int backtrace_reader(void* arg);
};



#endif /* AGENT_H_ */
