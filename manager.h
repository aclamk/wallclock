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

typedef void interruption_func(void);

extern "C"
void* agent_interface[4];



class probe_thread
{
public:
  probe_thread() {}
  ~probe_thread() {}

  user_regs_struct regs;

private:
  pid_t m_target{0};
  bool m_retry{false};
  uint64_t remote_context{0};
  /*
   *
   */
  int setup_execution_frame(user_regs_struct& regs);
  int setup_execution_frame(user_regs_struct& regs,
                            const user_regs_struct& previous_regs);
public:
  int setup_execution_func(user_regs_struct& regs,
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
  bool wait_return(uint64_t* arg1=nullptr, uint64_t* arg2=nullptr, uint64_t* arg3=nullptr);
  bool wait_stop(int& wstatus);
  bool grab_callback();
  void set_remote_context(uint64_t remote_context);
};

#endif /* MANAGER_H_ */
