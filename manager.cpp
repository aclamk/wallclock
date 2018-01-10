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

#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <cxxabi.h>
#include "agent.h"

int monitored_thread::inject_func(user_regs_struct& regs,
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
  regs.rip = agent_interface_remote._wc_inject + 2;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  return ret;
}


bool monitored_thread::seize(pid_t target)
{
  if (m_target != 0)
    detach();
  m_target = target;
  long ret;
  ret = ptrace(PTRACE_SEIZE, m_target, nullptr, nullptr);
  if (ret == 0) {
    if (!signal_interrupt()) ret = -1;
  }
  if (ret == 0) {
    if (waitpid(m_target, nullptr, 0) != m_target) ret = -1;
  }
  if (ret == 0) {
    ret = ptrace(PTRACE_SETOPTIONS, m_target, 0, PTRACE_O_TRACESYSGOOD);
  }
  if (ret == 0) {
    if (!cont()) ret = -1;
  }
  return (ret == 0);
}

bool monitored_thread::detach()
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

bool monitored_thread::signal_interrupt()
{
  long ret;
  ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool monitored_thread::cont()
{
  long ret;
  ret = ptrace(PTRACE_CONT, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool monitored_thread::syscall()
{
  long ret;
  ret = ptrace(PTRACE_SYSCALL, m_target, nullptr, nullptr);
  return (ret == 0);
}

bool monitored_thread::read_regs()
{
  long ret;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  return (ret == 0);
}

pid_t do_waitpid(pid_t pid, int* wstatus)
{
  pid_t wpid;
  uint32_t sleep_time = 1;
  uint32_t iter = 1;
  do {
    wpid = waitpid(pid, wstatus, WNOHANG);
    if (wpid == 0) {
      usleep(sleep_time);
      sleep_time += iter;
      iter++;
    }
  }
  while ((wpid == 0) && (iter < 100));
  return wpid;
}



bool monitored_thread::wait_return(uint64_t* arg1, uint64_t* arg2, uint64_t* arg3)
{
  bool result = false;
  int ret;
  user_regs_struct regs;
  do
  {
    if (!syscall()) {
      break;
    }
    int wstatus;
    if (do_waitpid(m_target, &wstatus) != m_target)
      break;
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

bool monitored_thread::wait_stop(int& wstatus, user_regs_struct& regs)
{
  long ret;
  pid_t ppp = do_waitpid(m_target, &wstatus);
  if (ppp != m_target)
    return false;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  if (ret != 0)
    return false;
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip-2), nullptr);
  if (errno != 0)
    return false;
  if ((code & 0xffff) == 0x050f)   //0x0f05 is SYSCALL
  {
    if (kill(m_target, SIGSTOP) != 0)
      return false;
    if (kill(m_target, SIGCONT) != 0)
      return false;
    ret = ptrace(PTRACE_SINGLESTEP, m_target, nullptr, nullptr);
    if (ret != 0)
      return false;
    ppp = do_waitpid(m_target, &wstatus);
    if (ppp != m_target)
      return false;
    ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
    if (ret != 0)
      return false;
  }
  return true;
}


void monitored_thread::set_remote_context(uint64_t remote_context)
{
  this->m_remote_context = remote_context;
}


bool monitored_thread::pause(user_regs_struct& regs)
{
  if (!signal_interrupt())
    return false;
  int wstatus = 0;
  if (!wait_stop(wstatus, regs))
    return false;
  return true;
}


bool monitored_thread::execute_remote(interruption_func* func,
                                  uint64_t* res1)
{
  user_regs_struct regs;
  if (!pause(regs))
    return false;
  if (0 != inject_func(regs, func, 0, 0, 0))
    return false;
  if (!wait_return(res1, nullptr, nullptr))
    return false;
  return true;
}



bool connect_client(uint64_t socket_hash)
{



  return false;
}
//bool trace_thread_new(pid_t pid, uint64_t& sc);


bool Manager::dump_tree(pid_t tid)
{
  uint64_t hit_count;
  uint64_t total_samples;
  uint64_t time_suspended;
  //uint64_t end_addr;
  //uint64_t ip_addr;
  std::string name;
  int8_t cmd = Agent::CMD_DUMP_TREE;
  bool res = false;
  //uint64_t sc_tmp;
  res = io.write(cmd);
  if (res) res = io.write(tid);

  std::string tid_name;
  res = io.read(tid_name);
  if (res) res = io.read(total_samples);
  if (res) res = io.read(time_suspended);
  std::cout << "Thread: " << tid << " (" << tid_name << ") - " << total_samples << " samples, time suspended=" <<
      time_suspended/(1000*1000) << "ms" << std::endl;
  std::cout << std::endl;
  std::vector<uint32_t> depths;
  depths.resize(1);
  depths[0] = 1;

  uint32_t depth;
  do
  {
    if (res) res = io.read(depth);
    if (res && depth != 0xffffffff) {
      if (res) res = io.read(name);
      if (res) res = io.read(hit_count);
      uint32_t child_count;// = children.size();
      if (res) res = io.read(child_count);
      if(depth <= depths.size())
        depths.resize(depth+1);
      depths[depth] = child_count;

      if(depth > 0)
      {
        for(size_t i = 0; i < depth - 1; i++)
        {
          if (depths[i] == 0)
            std::cout << "  ";
          else
            std::cout << "| ";
        }
        double d = hit_count * 100. / total_samples;
        char str[10];
        sprintf(str, "%2.2lf",d);
        int     status;
        char   *realname;

        realname = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
        std::cout << "+ " << str << "% " <<
            (status == 0 ? realname : name.c_str()) << std::endl;
        free(realname);
        depths[depth - 1]--;
      }
    }
  }
  while (res && depth != 0xffffffff);
  std::cout << std::endl;
  return res;
}



bool Manager::trace_attach(pid_t pid)
{
  bool res;
  int8_t cmd = Agent::CMD_TRACE_ATTACH;
  res = io.write(cmd);
  if (res) res = io.write(pid);
  return res;

}

bool Manager::probe()
{
  bool res;
  int8_t cmd = Agent::CMD_PROBE;
  uint32_t confirm;
  res = io.write(cmd);
  if (res) res = io.read(confirm);
  return res;
}
