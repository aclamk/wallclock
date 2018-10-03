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
#include <map>
#include "unix_io.h"
struct conveyor;
struct callstep;
class Agent;

class thread_sampling_ctx
{
  thread_sampling_ctx(size_t size);

public:
  ~thread_sampling_ctx();
  pid_t tid{0};
  std::atomic<bool> lock{false};
  std::atomic<bool> notified{false};
  conveyor* conv{nullptr};
  callstep* root{nullptr};
  std::atomic<uint32_t> backtrace_injected{0};
  std::atomic<uint32_t> backtrace_collected{0};
  std::atomic<uint64_t> pfunc{0};
  uint64_t time_suspended{0};
  /*creates sampling context for calling thread*/
  static thread_sampling_ctx* create();
  void peek();
  void consume();
  bool dump_tree(UnixIO& io,uint32_t total_samples, double suppress);
};


class Agent
{
  Agent();
  ~Agent();
public:
  static Agent* create();
  void add_thread(thread_sampling_ctx* sc);
  static int worker(void*, pid_t);
  bool scan_libraries(const std::string& excluded_library);
  bool load_symbols(const std::string& library, uint64_t begin);
  bool set_image();
  bool get_memory();
  std::pair<std::string, int64_t> get_symbol(uint64_t ip_addr);
  static int dl_iterate_phdr (int (*) (struct dl_phdr_info *, size_t, void *), void *);

  enum {
    CMD_TERMINATE=3,
    CMD_DUMP_TREE=4,
    CMD_TRACE_ATTACH=6,
    CMD_PROBE=7,
    CMD_READ_SYMBOLS=8,
    CMD_ADD_MEMORY=9,
    CMD_GET_MEMORY=10
  };

private:

  class Symbol
  {
  public:
    std::string name;
    int64_t size;
  };
  uint64_t image_begin;
  uint64_t image_size;
  std::vector<thread_sampling_ctx*> threads;
  UnixIO io;
  std::map<uint64_t, Symbol> symbols;
  static int (*dl_iterate_phdr_ptr) (
      int (*) (struct dl_phdr_info *, size_t, void *),
      void *);
  bool worker(pid_t pid);
  int read_command();
  bool dump_tree(thread_sampling_ctx* tsx, uint32_t total_samples, double suppress);
  bool dump_tree();
  bool indirect_backtrace();
  bool trace_attach();
  bool ptrace_attach(pid_t pid);
  bool ptrace_detach(pid_t pid);
  bool detach_threads();
  bool probe();

  friend int backtrace_reader(void* arg);
};



#endif /* AGENT_H_ */
