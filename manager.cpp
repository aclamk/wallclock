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

#include <string>
#include "agent.h"

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
  regs.rip = agent_interface_remote._wc_inject_backtrace;
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

  regs.rip = agent_interface_remote._wc_inject_backtrace_delayed;
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

  regs.rip = agent_interface_remote._wc_inject;
  if (ret == 0)
    ret = ptrace(PTRACE_SETREGS, m_target, nullptr, &regs);
  printf("ret=%d\n",ret);
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
  user_regs_struct regs;
  do
  {
    printf("wr1\n");
    if (!syscall()) {
      break;
    }
    printf("wr2\n");
    int wstatus;
    if (!wait_stop(wstatus, regs)) {
      break;
    }
    printf("wr3\n");

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
  printf("wr4\n");
  if(result)
    result = cont();
  return result;
}

bool probe_thread::wait_stop(int& wstatus, user_regs_struct& regs)
{
  long ret;
  printf("a\n");
  pid_t ppp = waitpid(m_target, &wstatus, 0);
  if (ppp != m_target)
    return false;
  printf("b\n");
  ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
  if (ret != 0)
    return false;
  printf("c\n");
  errno = 0;
  uint64_t code = ptrace(PTRACE_PEEKDATA, m_target, (uint64_t*)(regs.rip-2), nullptr);
  if (errno != 0)
    return false;
  if ((code & 0xffff) == 0x050f)   //0x0f05 is SYSCALL
  {
    ret = ptrace(PTRACE_SINGLESTEP, m_target, nullptr, nullptr);
    if (ret != 0)
      return false;
    printf("d\n");
    waitpid(m_target, &wstatus, 0);
    ret = ptrace(PTRACE_GETREGS, m_target, nullptr, &regs);
    printf("e\n");

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
    if (regs.rip != agent_interface_remote._wc_inject_backtrace &&
        regs.rip != agent_interface_remote._wc_inject_backtrace_delayed)
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

bool probe_thread::pause(user_regs_struct& regs)
{
  if (!signal_interrupt())
    return false;
  int wstatus;
  if (!wait_stop(wstatus, regs))
    return false;
  return true;
}



bool probe_thread::execute_remote(interruption_func* func,
                                  uint64_t* res1, uint64_t* res2, uint64_t* res3,
                                  uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
  user_regs_struct regs;
  if (!pause(regs))
    return false;
  printf("ER1\n");
  if (0 != setup_execution_func(regs, func, arg1, arg2, arg3))
    return false;
  printf("ER2\n");

  if (!wait_return(res1, res2, res3))
    return false;
  printf("ER3\n");
  return true;
}

bool probe_thread::execute_remote(interruption_func* func,
                                  uint64_t* res1, uint64_t* res2,
                                  uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
  uint64_t res3;
  return execute_remote(func, res1, res2, &res3, arg1, arg2, arg3);
}

bool probe_thread::execute_remote(interruption_func* func,
                                  uint64_t* res1,
                                  uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
  uint64_t res2;
  uint64_t res3;
  bool res =
   execute_remote(func, res1, &res2, &res3, arg1, arg2, arg3);
  printf("res=%d\n",res);
  return res;
}

bool probe_thread::execute_remote(interruption_func* func,
                                  uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
  uint64_t res1;
  uint64_t res2;
  uint64_t res3;
  return execute_remote(func, &res1, &res2, &res3, arg1, arg2, arg3);
}


bool connect_client(uint64_t socket_hash)
{



  return false;
}


int manager::connect_agent(uint64_t some_id)
{
  int conn_fd;
  struct sockaddr_un conn_addr;

  std::string unix_name{"@/wallclock/"};
  char hex[8*2+1];
  sprintf(hex, "%16.16lx", some_id);
  unix_name.append(hex);
  conn_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (conn_fd != -1)
  {
    memset(&conn_addr, 0, sizeof(struct sockaddr_un));
    conn_addr.sun_family = AF_UNIX;
    strncpy(conn_addr.sun_path, unix_name.c_str(), sizeof(conn_addr.sun_path) - 1);
    conn_addr.sun_path[0] = '\0';
    if (connect(conn_fd, (struct sockaddr *) &conn_addr, sizeof(struct sockaddr_un)) == 0)
    {
      this->conn_fd = conn_fd;
      return conn_fd;
    }
    else
      close(conn_fd);
  }
  return -1;
}

bool manager::read_bytes(void* ptr, size_t size)
{
  int res;
  res = read(conn_fd, ptr, size);
  return res == size;
}

bool manager::write_bytes(const void* ptr, size_t size)
{
  int res;
  res = write(conn_fd, ptr, size);
  return res == size;
}
//bool trace_thread_new(pid_t pid, uint64_t& sc);

bool manager::trace_thread_new(uint64_t& sc)
{
  uint8_t cmd = agent::CMD_TRACE_THREAD_NEW;
  bool res = false;
  uint64_t sc_tmp;
  if (write_bytes(&cmd, sizeof(uint8_t))) {
    if (read_bytes(&sc_tmp, sizeof(uint64_t)))
    {
      sc = sc_tmp;
      res = true;
    }
  }
  return res;
}



#if 0
int
       main(int argc, char *argv[])
       {
           int sfd, cfd;
           struct sockaddr_un my_addr, peer_addr;
           socklen_t peer_addr_size;

           sfd = socket(AF_UNIX, SOCK_STREAM, 0);
           if (sfd == -1)
               handle_error("socket");

           memset(&my_addr, 0, sizeof(struct sockaddr_un));
                               /* Clear structure */
           my_addr.sun_family = AF_UNIX;
           strncpy(my_addr.sun_path, MY_SOCK_PATH,
                   sizeof(my_addr.sun_path) - 1);

           if (bind(sfd, (struct sockaddr *) &my_addr,
                   sizeof(struct sockaddr_un)) == -1)
               handle_error("bind");

           if (listen(sfd, LISTEN_BACKLOG) == -1)
               handle_error("listen");

           /* Now we can accept incoming connections one
              at a time using accept(2) */

           peer_addr_size = sizeof(struct sockaddr_un);
           cfd = accept(sfd, (struct sockaddr *) &peer_addr,
                        &peer_addr_size);
           if (cfd == -1)
               handle_error("accept");

           /* Code to deal with incoming connection(s)... */

           /* When no longer required, the socket pathname, MY_SOCK_PATH
              should be deleted using unlink(2) or remove(3) */
       }


#endif

