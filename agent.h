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
  pid_t tid;
  std::atomic<bool> lock{0};
  conveyor* conv{nullptr};
  callstep* root{nullptr};
  std::atomic<uint32_t> backtrace_injected{0};
  std::atomic<uint32_t> backtrace_collected{0};
  std::atomic<uint64_t> pfunc{0};
  /*creates sampling context for calling thread*/
  static thread_sampling_ctx* create();
  void peek();
  void consume();
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
    CMD_DUMP_TREE=4,
    CMD_INDIRECT_BACKTRACE=5,
    CMD_TRACE_ATTACH=6,
    CMD_PROBE=7
  };

  static int worker(void*);
  UnixIO io;
private:

  bool worker();

  int read_command();

  bool trace_thread_new();
  bool dump_tree(thread_sampling_ctx* tsx);
  bool dump_tree();
  bool indirect_backtrace();
  bool trace_attach();
  bool ptrace_attach(pid_t pid);

  bool probe();
  friend int backtrace_reader(void* arg);
};



#endif /* AGENT_H_ */
