/*
 * loader.cpp
 *
 *  Created on: Dec 7, 2017
 *      Author: adam
 */
#include <fstream>
#include <sys/types.h>
#include <sstream>
#include <iostream>
#include <syscall.h>
#include <unistd.h>
#include "loader.h"
#include <string.h>
#include <assert.h>
#include "manager.h"
#include <linux/ptrace.h>

#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/uio.h>

extern RemoteAPI agent_interface;
RemoteAPI agent_interface_remote;


void* locate_library(pid_t pid, const std::string& library_name)
{
  std::string proc_maps_name = "/proc/" + std::to_string(pid) + "/maps";

  std::ifstream proc_maps(proc_maps_name);
  if (!proc_maps.good())
    return nullptr;

  while (!proc_maps.eof())
  {
    std::string line;

    std::string addr_begin;
    std::string addr_end;
    std::string perms;
    std::string offset;
    std::string dev;
    std::string inode;
    std::string pathname;

    if (std::getline(proc_maps,line))
    {
      std::stringstream is(line);
      if (!std::getline(is, addr_begin, '-')) continue;
      if (!std::getline(is, addr_end, ' ')) continue;
      if (!std::getline(is, perms, ' ')) continue;
      if (!std::getline(is, offset, ' ')) continue;
      if (!std::getline(is, dev, ' ')) continue;
      if (!std::getline(is, inode, ' ')) continue;
      while(is.peek() == ' ')
        is.get();
      if (!std::getline(is, pathname)) continue;
    }
    if (pathname.find(library_name)!=pathname.npos && perms == "r-xp")
    {
      return (void*)strtoll(addr_begin.c_str(), nullptr, 16);
    }
  }
  return nullptr;
}

pid_t find_leader_thread(pid_t pid)
{
  pid_t leader_pid = 0;
  std::string proc_status_name = "/proc/" + std::to_string(pid) + "/status";

  std::ifstream proc_status(proc_status_name);
  if (!proc_status.good())
    return 0;
  while (!proc_status.eof())
  {
    std::string line;

    std::string tag;
    std::string value;

    if (std::getline(proc_status,line))
    {
      std::stringstream is(line);
      if (!std::getline(is, tag, '\t')) continue;
      if (!std::getline(is, value, '\n')) continue;
      if (tag == "Tgid:")
        leader_pid = (pid_t)strtoll(value.c_str(), nullptr, 0);
    }
  }
  return leader_pid;
}

extern RemoteAPI agent_interface;

bool init_agent_so(pid_t remote, pid_t remote_leader)
{
  void* my_agent_so = locate_library(syscall(SYS_gettid), "agent.so");
  void* remote_agent_so = locate_library(remote, "agent.so");
  assert(my_agent_so != nullptr);
  if (remote_agent_so == nullptr) {
    std::cout << "remote does not have agent.so library" << std::endl;
    //exit(-1);
    return false;
  }
  if (memcmp(agent_interface.sanity_marker,"AGENTAPI",8) != 0) {
    return false;
  }

  int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;

  agent_interface_remote = agent_interface;
  agent_interface_remote._init_agent += diff;
  agent_interface_remote._wc_inject += diff;

  monitored_thread pt;
  if (!pt.seize(remote))
  {
    std::cerr << "Target " << remote << " cannot be sized" << std::endl;
    return false;
  }
  if (!pt.execute_remote((interruption_func*)agent_interface_remote._init_agent, (uint64_t)remote_leader))
  {
    std::cerr << "failed to execute remote agent" << std::endl;
    return false;
  }
  if (!pt.detach())
    return false;

  return true;
}

bool load_binary_agent(pid_t remote, pid_t remote_leader)
{
  monitored_thread pt;
  int conn_fd;
  if (!pt.seize(remote)) {
    std::cerr << "Target " << remote << " cannot be sized" << std::endl;
    return false;
  }

  int res;
  char* local_image;
  struct stat buf;
  int fd = open("pagent.rel",O_RDONLY);
  if (fd >= 0) {
    res = fstat(fd, &buf);
    if (res == 0) {
      local_image = new char[buf.st_size];
      //ptr = (char*)mmap((void*)0x10000000, buf.st_size*2, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANON|MAP_FIXED, -1, 0);
      res = read(fd, local_image, buf.st_size);
    }
    close (fd);
    if (res != buf.st_size) return false;
  }

  user_regs_struct regs;
  uint64_t syscall_rip;
  if (!pt.locate_syscall(&syscall_rip)) return false;
  if (!pt.pause_outside_syscall()) return false;
  user_regs_struct mmap_result;
  pt.inject_syscall(syscall_rip,[&buf](user_regs_struct& regs){
    regs.rax = SYS_mmap;
    regs.rdi = 0;
    regs.rsi = buf.st_size*2;
    regs.rdx = PROT_READ|PROT_WRITE|PROT_EXEC;
    regs.r10 = MAP_PRIVATE|MAP_ANON|MAP_32BIT;
    regs.r8 = -1;
    regs.r9 = 0;
  }, mmap_result);
  printf("rax=%llx\n", mmap_result.rax);
  if (mmap_result.rax == -1) return false;
  char* remote_image = (char*) mmap_result.rax;
  struct iovec local_iov;
  struct iovec remote_iov;
  local_iov.iov_base = local_image;
  local_iov.iov_len = buf.st_size;
  remote_iov.iov_base = remote_image;
  remote_iov.iov_len = buf.st_size;

  res = process_vm_writev(remote, &local_iov, 1, &remote_iov, 1, 0);
  printf("res=%d remote_leader=%d\n",res, remote_leader);
  pt.execute_init((uint64_t)remote_image, remote_leader);
  //uint64_t unix_id;
  pt.wait_return();
  pt.detach();

  //int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;

  agent_interface_remote = agent_interface;
  agent_interface_remote._init_agent = 0x10000000;
  //agent_interface_remote._wc_inject += diff;

  return true;
}



bool init_agent_interface(Manager& mgr, pid_t remote, bool use_agent_so)
{
  pid_t remote_leader = find_leader_thread(remote);

  //first try to connect to existing agent
  UnixIO io_m;
  int server_fd = io_m.server(remote_leader);
  printf("check_server_fd=%d\n",server_fd);
  if (server_fd != -1) {
    close(server_fd);
    if (use_agent_so) {
      bool b = init_agent_so(remote, remote_leader);
      if (!b) {
        std::cerr << "Unable to use remote agent.so" << std::endl;
        return false;
      }
    } else {
      bool b = load_binary_agent(remote, remote_leader);
      if (!b) {
        std::cerr << "Failed to load wallclock agent to remote" << std::endl;
        return false;
      }
    }
  }
  long ret;
  int conn_fd;

  int count = 0;
  conn_fd = -1;
  while (count < 30 && conn_fd ==-1)
  {
    conn_fd = mgr.io.connect(remote_leader);
    count++;
    usleep(100*1000);
  }
  if (conn_fd == -1)
    return false;

  return true;
}



