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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

bool monitored_thread::execute_init(uint64_t init_addr, uint64_t connection_id)
{
  int ret = 0;
  user_regs_struct regs;
  if (!read_regs(regs)) return false;
  regs.rsp -= ( 128 + 8);
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rdi);
  regs.rsp -= 8;
  if (ret == 0)
    ret = ptrace(PTRACE_POKEDATA, m_target, (uint64_t*)(regs.rsp), (void*)regs.rip);
  regs.rdi = connection_id;//0xfee1fab; //this will mean init was called from remote
  regs.rip = init_addr;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  //cont();
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
    int wstatus;
    if (!wait_status(&wstatus)) ret = -1;
  }
  if (ret == 0) {
    ret = ptrace(PTRACE_SETOPTIONS, m_target, 0, PTRACE_O_TRACESYSGOOD);
  }
  if (ret == 0) {
    if (!cont()) ret = -1;
  }
  return (ret == 0);
}

bool monitored_thread::attach(pid_t target)
{
  if (m_target != 0)
    detach();
  m_target = target;
  long ret;
  ret = ptrace(PTRACE_ATTACH, m_target, nullptr, nullptr);
  if (ret == 0) {
    int wstatus;
    if (!wait_status(&wstatus)) {
      ret = -1;
    }
  }
  if (ret == 0) {
    ret = ptrace(PTRACE_SETOPTIONS, m_target, 0, PTRACE_O_TRACESYSGOOD);
  }
  return (ret == 0);
}

bool monitored_thread::detach()
{
  long ret;
  ret = ptrace(PTRACE_INTERRUPT, m_target, nullptr, nullptr);
  if (ret == 0)
  {
    int wstatus;
    if (!wait_status(&wstatus)) ret = -1;

  }
  ret = ptrace(PTRACE_DETACH, m_target, nullptr, nullptr);
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
  ret = ptrace(PTRACE_CONT, m_target, 0, nullptr);
  return (ret == 0);
}

bool monitored_thread::syscall()
{
  long ret;
  ret = ptrace(PTRACE_SYSCALL, m_target, 0, nullptr);
  return (ret == 0);
}

bool monitored_thread::single_step()
{
  long ret;
  ret = ptrace(PTRACE_SINGLESTEP, m_target, 0, nullptr);
  return (ret == 0);
}


bool monitored_thread::read_regs()
{
  long ret;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  return (ret == 0);
}

bool monitored_thread::read_regs(user_regs_struct& regs)
{
  long ret;
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  return (ret == 0);
}

bool monitored_thread::write_regs(const user_regs_struct& regs)
{
  long ret;
  ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  this->regs = regs;
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

bool monitored_thread::wait_status(int* wstatus, uint32_t timeout)
{
  pid_t wpid;
  uint32_t sleep_total = 0;
  uint32_t sleep_time = 1000;
  uint32_t iter = 1;
  do {
    wpid = waitpid(m_target, wstatus, WNOHANG);
    if (wpid == 0) {
      usleep(sleep_time);
      sleep_time += iter*1000;
      sleep_total += sleep_time;
      iter++;
    }
  }
  while ((wpid == 0) && (sleep_total < timeout));
  return wpid == m_target;
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

/*in case of error, default to true*/
bool monitored_thread::in_syscall()
{
  std::string syscall_proc = "/proc/" + std::to_string(m_target) + "/syscall";
  char syscall_buffer[1024];
  int syscall_no;
  int fd = open(syscall_proc.c_str(), O_RDONLY);
  if (fd < 0)
    return true;
  int r = read(fd, syscall_buffer, 1023);
  close(fd);
  if (r <= 0)
    return true;
  syscall_buffer[1023]=0;
  syscall_no = strtol(syscall_buffer, nullptr, 0);
  return (syscall_no != -1);
}

bool monitored_thread::pause_outside_syscall()
{
  if (!in_syscall()) {
    return true;
  }
  if (single_step()) {
    int wstatus;
    if (wait_status(&wstatus)) {
      if (!in_syscall()) {
        return true;
      }
    } else {
      kill(m_target, SIGSTOP);
      kill(m_target, SIGCONT);
      wait_status(&wstatus);
    }
    return false;
  }
  return false;
}


bool monitored_thread::inject_syscall(std::function<void(user_regs_struct&)> prepare_regs, user_regs_struct& result)
{
  uint64_t syscall_rip;
  user_regs_struct saved_regs;
  if (!read_regs(saved_regs)) return false;
  user_regs_struct aregs = regs;
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(saved_regs.rip - 2), nullptr);
  if (errno != 0) return false;
  if ((code & 0xffff) != 0x050f) return false;  //0x0f05 is SYSCALL
  syscall_rip = saved_regs.rip - 2;
  //we seem to be in proper place in code
  //step out of syscall
  do {
    int wstatus;
    if (!single_step()) return false;
    if (!wait_status(&wstatus)) return false;
    if (!WIFSTOPPED(wstatus)) return false;
    if (!read_regs(saved_regs)) return false;
  } while (saved_regs.rip - 2 == syscall_rip);

  aregs = saved_regs;
  aregs.rip = syscall_rip;
  prepare_regs(aregs);
  if (!write_regs(aregs)) return false;
  do {
    int wstatus;
    if (!single_step()) return false;
    if (!wait_status(&wstatus)) return false;
    if (!WIFSTOPPED(wstatus)) return false;
    if (!read_regs(aregs)) return false;
  } while (aregs.rip == syscall_rip);
  result = aregs;
  if (!write_regs(saved_regs)) return false;
  return true;
}


bool monitored_thread::inject_syscall(uint64_t syscall_rip,
                                      std::function<void(user_regs_struct&)> prepare_regs,
                                      user_regs_struct& result)
{
  user_regs_struct saved_regs;
  user_regs_struct regs;
  if (!read_regs(saved_regs)) return false;

  regs = saved_regs;
  regs.rip = syscall_rip;
  prepare_regs(regs);
  if (!write_regs(regs)) return false;
  do {
    int wstatus;
    if (!single_step()) return false;
    if (!wait_status(&wstatus)) return false;
    if (!WIFSTOPPED(wstatus)) return false;
    if (!read_regs(regs)) return false;
  } while (regs.rip == syscall_rip);
  result = regs;
  if (!write_regs(saved_regs)) return false;
  return true;
}

bool monitored_thread::locate_syscall(uint64_t* syscall_eip)
{
  if(!signal_interrupt()) return false;
  user_regs_struct regs;
  int wstatus;
  if(!wait_status(&wstatus)) return false;
  if(!read_regs(regs)) return false;
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip - 2), nullptr);
  if (errno != 0) return false;
  if ((code & 0xffff) == 0x050f) {
    //easy success
    *syscall_eip = regs.rip - 2;
    cont();
    return true;
  }
  if(!syscall()) return false;
  if (kill(m_target, SIGSTOP) != 0)
    return false;
  if (kill(m_target, SIGCONT) != 0)
    return false;
  if(!wait_status(&wstatus)) return false;
  if(!read_regs(regs)) return false;
  errno = 0;
  code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip - 2), nullptr);
  if (errno != 0) return false;
  if ((code & 0xffff) == 0x050f) {
    //easy success
    *syscall_eip = regs.rip - 2;
    cont();
    return true;
  }
  cont();
  return false;
}

bool monitored_thread::execute_remote(interruption_func* func,
                                  uint64_t arg1)
{
  user_regs_struct regs;
  read_regs(regs);
  if (0 != inject_func(regs, func, arg1, 0, 0))
    return false;
  if (!wait_return(nullptr, nullptr, nullptr))
    return false;
  return true;
}



bool connect_client(uint64_t socket_hash)
{



  return false;
}

bool Manager::dump_tree(std::ostream& output, pid_t tid, double suppress)
{
  uint64_t hit_count;
  uint64_t total_samples;
  uint64_t time_suspended;
  std::string name;
  int8_t cmd = Agent::CMD_DUMP_TREE;
  bool res = false;
  res = io.write(cmd);
  if (res) res = io.write(tid);
  if (res) res = io.write(suppress);

  std::string tid_name;
  res = io.read(tid_name);
  if (res) res = io.read(total_samples);
  if (res) res = io.read(time_suspended);
  output << "Thread: " << tid << " (" << tid_name << ") - " << total_samples << " samples, time suspended=" <<
      time_suspended/(1000*1000) << "ms" << std::endl;
  output << std::endl;
  std::vector<uint32_t> depths;

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
            output << "  ";
          else
            output << "| ";
        }
        double d = hit_count * 100. / total_samples;
        char str[10];
        sprintf(str, "%2.2lf",d);
        int     status;
        char   *realname;

        realname = abi::__cxa_demangle(name.c_str(), 0, 0, &status);
        output << "+ " << str << "% " <<
            (status == 0 ? realname : name.c_str()) << std::endl;
        free(realname);
        depths[depth - 1]--;
      }
    }
  }
  while (res && depth != 0xffffffff);
  output << std::endl;
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
  if (verbose_level >= 5) std::cerr << "probe " << std::flush;
  int8_t cmd = Agent::CMD_PROBE;
  uint32_t confirm;
  res = io.write(cmd);
  if (res) res = io.read(confirm);
  if (verbose_level >= 5) std::cerr << "res=" << res << std::endl;
  return res;
}

bool Manager::read_symbols()
{
  bool res;
  int8_t cmd = Agent::CMD_READ_SYMBOLS;
  uint32_t confirm;
  res = io.write(cmd);
  if (res) res = io.read(confirm);
  return res;
}

bool Manager::set_image(uint64_t begin, uint64_t size)
{
  bool res;
  int8_t cmd = Agent::CMD_ADD_MEMORY;
  res = io.write(cmd);
  if (res) res = io.write(begin);
  if (res) res = io.write(size);
  return res;
}

bool Manager::terminate()
{
  bool res;
  int8_t cmd = Agent::CMD_TERMINATE;
  res = io.write(cmd);
  uint32_t response;
  if (res) res = io.read(response);
  if (res) res = (response == 0xdeadbeef);
  return res;

}

bool Manager::get_memory(std::vector<std::pair<uint64_t, uint64_t>>& regions)
{
  bool res;
  int8_t cmd = Agent::CMD_GET_MEMORY;
  res = io.write(cmd);
  uint32_t region_count;
  if (res) res = io.read(region_count);
  if (res) {
    regions.clear();
    for (size_t i=0; res && i<region_count; i++) {
      uint64_t addr;
      uint64_t size;
      if (res) res = io.read(addr);
      if (res) res = io.read(size);
      if (res) regions.emplace_back(addr, size);
    }
  }
  return res;
}
