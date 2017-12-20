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
typedef void interruption_func(void);

//extern "C"
//void* agent_interface[4];



class monitored_thread
{
public:
  monitored_thread() {}
  ~monitored_thread() {}

  user_regs_struct regs;

private:
  pid_t m_target{0};
  bool m_retry{false};
  uint64_t remote_context{0};
  /*
   *
   */
  int inject_backtrace(user_regs_struct& regs);
  int inject_backtrace(user_regs_struct& regs,
                            const user_regs_struct& previous_regs);

  int inject_func(user_regs_struct& regs,
                            interruption_func func,
                            uint64_t arg1 = 0,
                            uint64_t arg2 = 0,
                            uint64_t arg3 = 0);
public:
  bool seize(pid_t target);
  bool detach();
  bool signal_interrupt();
  bool cont();
  bool syscall();
  bool read_regs();
  bool wait_return(uint64_t* arg1=nullptr,
                   uint64_t* arg2=nullptr,
                   uint64_t* arg3=nullptr);
  bool wait_stop(int& wstatus, user_regs_struct& regs);
  bool grab_callback();
  void set_remote_context(uint64_t remote_context);

  bool pause(user_regs_struct& regs);

  bool execute_remote(interruption_func* func,
                      uint64_t* res1);

};

class Manager
{
public:
  UnixIO io;

  bool trace_thread_new(uint64_t& sc);
  bool dump_tree(uint64_t sc);

};

void execute_command();


#endif /* MANAGER_H_ */
