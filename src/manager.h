/*
 * manager.h
 *
 *  Created on: Dec 5, 2017
 *      Author: adam
 */

#ifndef WCLK_MANAGER_H_
#define WCLK_MANAGER_H_

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <inttypes.h>
#include "loader.h"
#include "unix_io.h"
#include <atomic>
#include <functional>
#include <vector>
typedef void interruption_func(void);

typedef uint64_t remote_sampling_ctx;

class monitored_thread
{
public:
  /*
  monitored_thread() {}
  monitored_thread(monitored_thread&& mt):
    m_target(mt.m_target),
    m_retry(mt.m_retry),
    m_remote_context(mt.m_remote_context) {}
//  monitored_thread(monitored_thread& mt):
//    m_target(mt.m_target) {}
  ~monitored_thread() {}
*/
  user_regs_struct regs;

public:
  pid_t m_target{0};
  bool m_retry{false};
  remote_sampling_ctx m_remote_context{0};
  //std::atomic<uint32_t> m_backtrace_inject_requests{0};

  int inject_backtrace(user_regs_struct& regs);
  int inject_backtrace(user_regs_struct& regs,
                            const user_regs_struct& previous_regs);

  int inject_func(user_regs_struct& regs,
                            interruption_func func,
                            uint64_t arg1 = 0,
                            uint64_t arg2 = 0,
                            uint64_t arg3 = 0);
public:
  bool attach(pid_t target);
  bool seize(pid_t target);
  bool detach();
  bool signal_interrupt();
  bool cont();
  bool syscall();
  bool single_step();
  bool read_regs();
  bool read_regs(user_regs_struct& regs);
  bool write_regs(const user_regs_struct& regs);
  bool wait_return(uint64_t* arg1=nullptr,
                   uint64_t* arg2=nullptr,
                   uint64_t* arg3=nullptr);
  bool wait_stop(int& wstatus, user_regs_struct& regs);
  bool grab_callback();
  void set_remote_context(uint64_t remote_context);

  bool pause(user_regs_struct& regs);
  bool pause_outside_syscall();
  bool in_syscall();
  bool execute_remote(interruption_func* func,
                      uint64_t arg1);
  bool wait_status(int* wstatus, uint32_t timeout = 100000);
  bool inject_syscall(std::function<void(user_regs_struct&)> prepare_regs,
                      user_regs_struct& result);
  bool inject_syscall(uint64_t syscall_rip,
                      std::function<void(user_regs_struct&)> prepare_regs,
                      user_regs_struct& result);
  bool locate_syscall(uint64_t* syscall_rip);
  bool execute_init(uint64_t init_addr, uint64_t connection_id);
};

class Manager
{
public:
  UnixIO io;

  bool trace_thread_new(uint64_t& sc);
  bool dump_tree(std::ostream& output, pid_t tid, double suppress);

  bool indirect_backtrace(uint64_t sc, uint64_t rip, uint64_t rbp, uint64_t rsp);
  bool trace_attach(pid_t pid);
  bool probe();
  bool read_symbols();
  bool set_image(uint64_t begin, uint64_t size);
  bool get_memory(std::vector<std::pair<uint64_t, uint64_t>>& regions);
};

void execute_command();

#endif /* MANAGER_H_ */
