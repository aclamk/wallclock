/*
 * manager.cpp
 *
 *  Created on: Dec 5, 2017
 *      Author: adam
 */

#include "manager.h"

#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>

#include "loader.h"
#if 0
extern "C"
void _wrapper(void);

extern "C"
void _wrapper_regs_provided(void);

extern "C"
void _wrapper_to_func(void);
#endif




int probe_thread::setup_execution_frame(user_regs_struct& regs)
{
  //make frame big enough to skip "scratch area"
  //x86_64-abi section 3.2.2 The Stack Frame, declares sp+0 .. sp+128 as reserved
  int ret;
  regs.rsp -= (128 + 8);
  ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)remote_context);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);
  regs.rip = (uint64_t)_wrapper_call;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  return ret;
}

int probe_thread::setup_execution_frame(user_regs_struct& regs,
                                        const user_regs_struct& previous_regs)
{
  int ret;
  regs.rsp -= (128 + 8);
  ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rip);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rbp);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)previous_regs.rsp);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)remote_context);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);

  regs.rip = (uint64_t)_wrapper_regs_provided_call;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  return ret;
}

int probe_thread::setup_execution_func(user_regs_struct& regs,
                                       interruption_func func,
                                       uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
  int ret;
  regs.rsp -= ( 128 + 8);
  ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)func);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg1);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg2);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)arg3);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);

  regs.rip = (uint64_t)_wrapper_to_func_call;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  return ret;
}


bool probe_thread::seize(pid_t target)
{
  if (m_target != 0)
    detach();
  m_target = target;
  long ret;
  ret = ptrace(PTRACE_SEIZE, m_target, nullptr, nullptr);

  signal_interrupt();
  waitpid(m_target, nullptr, 0);
  ret = ptrace(PTRACE_SETOPTIONS, m_target, 0, PTRACE_O_TRACESYSGOOD);
  if (ret != 0)
    return false;
  cont();
  return (ret == 0);
}

bool probe_thread::detach()
{
  long ret;
  ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
  if (ret == 0)
  {
    waitpid(m_target, nullptr, 0);
    ret = ptrace(PTRACE_DETACH, m_target, nullptr, nullptr);
  }
  m_target = 0;
  return (ret == 0);
}

bool probe_thread::signal_interrupt()
{
  long ret;
  ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool probe_thread::cont()
{
  long ret;
  ret = ptrace(PTRACE_CONT, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool probe_thread::syscall()
{
  long ret;
  ret = ptrace(PTRACE_SYSCALL, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool probe_thread::read_regs()
{
  long ret;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  return (ret == 0);
}

bool probe_thread::wait_return(uint64_t* arg1, uint64_t* arg2, uint64_t* arg3)
{
  bool result = false;
  int ret;
  do
  {
    if (!syscall()) {
      break;
    }
    int wstatus;
    if (!wait_stop(wstatus)) {
      break;
    }

    if (WIFSTOPPED(wstatus) && ((WSTOPSIG(wstatus) & ~0x80) == (SIGTRAP | 0x00)))
    {
      ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
      if (ret != 0) {
        break;
      }
      if (regs.orig_rax == -1)
      {
        if (arg1)
          *arg1 = regs.rdi;
        if (arg2)
          *arg2 = regs.rsi;
        if (arg3)
          *arg3 = regs.rdx;
        result = true;
        break;
      }
    }
  }
  while (true);
  if(result)
    result = cont();
  return result;
}

bool probe_thread::wait_stop(int& wstatus)
{
  long ret;
  pid_t ppp = waitpid(m_target, &wstatus, 0);
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  if (ret != 0)
    return false;
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip-2), nullptr);
  if (errno != 0)
    return false;
  if ((code & 0xffff) == 0x050f)   //0x0f05 is SYSCALL
      {
    ret = ptrace(PTRACE_SINGLESTEP, m_target, nullptr, nullptr);
    if (ret != 0)
      return false;
    waitpid(m_target, &wstatus, 0);
    ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
    if (ret != 0)
      return false;
      }
  return true;
}


bool probe_thread::grab_callback()
{
  long ret;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  if (ret != 0)
    return false;
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip-2), nullptr);
  if (errno != 0)
    return false;
  if ((code & 0xffff ) == 0x050f)   //0x0f05 is SYSCALL
  {
    ret = ptrace(PTRACE_SINGLESTEP, m_target, nullptr, nullptr);
    if (ret == 0)
    {
      int wstatus;
      waitpid(m_target, &wstatus, 0);
      user_regs_struct regs_as;
      ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs_as);
      if (ret == 0)
      {
        ret = setup_execution_frame(regs_as, regs);
      }
    }
  }
  else
  {
    ret = 0;
    if (regs.rip != (uint64_t)_wrapper_call &&
        regs.rip != (uint64_t)_wrapper_regs_provided_call)
    {
      ret = setup_execution_frame(regs);
    }
  }
  if (ret == 0)
  {
    if (!cont())
    {
      ret = -1;
    }
  }
  return (ret == 0);
}

void probe_thread::set_remote_context(uint64_t remote_context)
{
  this->remote_context = remote_context;
}

