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
#include <vector>
#include <set>
#include <dirent.h>
#include <thread>
#include <dlfcn.h>
#include <iomanip>
extern RemoteAPI agent_interface;
RemoteAPI agent_interface_remote;

extern int verbose_level;

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

std::set<pid_t> list_threads_in_group(pid_t pid)
{
  std::set<pid_t> threads;
  std::string proc_tasks = "/proc/" + std::to_string(pid) + "/task";
  DIR *dir;
  struct dirent *ent;
  if ((dir = opendir (proc_tasks.c_str())) != nullptr)
  {
    while ((ent = readdir (dir)) != NULL)
    {
      char* endptr;
      int tid = strtol(ent->d_name, &endptr, 10);
      if (*endptr== '\0')
      {
        threads.insert(tid);
      }
    }
    closedir (dir);
  }
  return threads;
}

monitored_thread pause_outside_syscall(pid_t remote_leader)
{
  std::set<pid_t> threads = list_threads_in_group(remote_leader);
  std::vector<monitored_thread> mt;
  monitored_thread pt;
  while (pt.m_target == 0) {
    for (auto i = threads.begin(); i != threads.end(); i++) {
      if (pt.attach(*i)) {
        if (pt.pause_outside_syscall()) {
          break;
        }
        pt.detach();
      }
    }
  }
  return pt;
}

bool init_agent_so(pid_t remote, pid_t remote_leader)
{
  void* handle = dlopen("libagent.so",RTLD_NOW);
  if (!handle) {
    if (verbose_level >= 2)
      std::cout << "Unable to load libagent.so ." << std::endl;
    return false;
  }
  void* sym = dlsym(handle, "agent_interface");
  if (!sym)
    return false;
  RemoteAPI* agent_interface = (RemoteAPI*)sym;
  void* my_agent_so = locate_library(syscall(SYS_gettid), "libagent.so");
  void* remote_agent_so = locate_library(remote, "libagent.so");
  assert(my_agent_so != nullptr);
  if (remote_agent_so == nullptr) {
    if (verbose_level >= 2)
      std::cout << "Remote does not have libagent.so library." << std::endl;
    return false;
  }
  if (memcmp(agent_interface->sanity_marker,"AGENTAPI",8) != 0) {
    if (verbose_level >= 2)
      std::cerr << "Target sanity check for libagent.so failed." << std::endl;
    return false;
  }

  int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;
  agent_interface_remote = *agent_interface;
  agent_interface_remote._init_agent += diff;
  agent_interface_remote._wc_inject += diff;

  monitored_thread pt = pause_outside_syscall(remote_leader);
  if (pt.m_target == 0) {
    if (verbose_level >= 2)
      std::cerr << "Target " << remote << " cannot be sized." << std::endl;
    return false;
  }
  if (!pt.execute_remote((interruption_func*)agent_interface_remote._init_agent,
                         (uint64_t)remote_leader)) {
    if (verbose_level >= 2)
      std::cerr << "Failed attempt to execute remote agent in libagent.so ." << std::endl;
    return false;
  }
  return true;
}

extern "C" {
  extern char _binary_relagent_start[0];
  extern char _binary_relagent_end[0];
}

bool load_binary_agent(pid_t remote, pid_t remote_leader, bool pause_for_ptrace,
                       std::pair<uint64_t, uint64_t>& addr)
{
  uint64_t syscall_rip = 0;
  bool sized_something = false;
  std::set<pid_t> threads = list_threads_in_group(remote_leader);

  for(auto i = threads.begin(); i != threads.end(); i++)
  {
    monitored_thread pt;
    if (pt.seize(*i)) {
      sized_something = true;
      pt.locate_syscall(&syscall_rip);
      pt.detach();
      break;
    }
  }

  if (sized_something == false) {
    std::cout << "Unable to attach to threads. "
        "Check setting of /proc/sys/kernel/yama/ptrace_scope." << std::endl;
    return false;
  }

  if (syscall_rip == 0) {
    if (verbose_level >= 2)
      std::cout << "Unable to pause process and find a syscall." << std::endl;
    return false; //was not able to locate place of rip
  }

  monitored_thread pt = pause_outside_syscall(remote_leader);
  if (pt.m_target == 0) {
    if (verbose_level >= 2)
      std::cout << "Unable to pause remote process outside of syscall." << std::endl;
    return false; //was not able to connect to any of threads
  }
  int res;

  user_regs_struct regs;
  user_regs_struct mmap_result;
  pt.inject_syscall(syscall_rip,[&](user_regs_struct& regs){
    regs.rax = SYS_mmap;
    regs.rdi = 0;
    regs.rsi = _binary_relagent_end - _binary_relagent_start;
    regs.rdx = PROT_READ|PROT_WRITE|PROT_EXEC;
    regs.r10 = MAP_PRIVATE|MAP_ANON|MAP_32BIT;
    regs.r8 = -1;
    regs.r9 = 0;
  }, mmap_result);
  if (mmap_result.rax == -1) {
    if (verbose_level >= 2)
      std::cout << "Allocating mmap for remote agent failed." << std::endl;
    return false;
  }
  char* remote_image = (char*) mmap_result.rax;

  if (verbose_level>=4)
    std::cout << "Remote allocated memory for agent: " << (void*)remote_image << "-"
    << (void*)(remote_image + (_binary_relagent_end - _binary_relagent_start)) << std::endl;

  struct iovec local_iov;
  struct iovec remote_iov;
  local_iov.iov_base = _binary_relagent_start;
  local_iov.iov_len = _binary_relagent_end - _binary_relagent_start;
  remote_iov.iov_base = remote_image;
  remote_iov.iov_len = _binary_relagent_end - _binary_relagent_start;

  addr = {mmap_result.rax, _binary_relagent_end - _binary_relagent_start};
  res = process_vm_writev(remote, &local_iov, 1, &remote_iov, 1, 0);
  pt.execute_init((uint64_t)remote_image, pause_for_ptrace?remote_leader|0x100000000:remote_leader);
  pt.wait_return();
  pt.detach();
  if (verbose_level>=1)
    std::cout << "Agent loaded to address " << (void*)remote_image << std::endl;
  return true;
}


bool unmap_memory(pid_t remote_leader,
                  const std::vector<std::pair<uint64_t, uint64_t>>& regions)
{
  uint64_t syscall_rip = 0;
  std::set<pid_t> threads = list_threads_in_group(remote_leader);

  for(auto i = threads.begin(); i != threads.end(); i++)
  {
    monitored_thread pt;
    if (pt.seize(*i)) {
      pt.locate_syscall(&syscall_rip);
      pt.detach();
      break;
    }
  }
  if (syscall_rip == 0)
    return false; //was not able to locate place of rip

  monitored_thread pt = pause_outside_syscall(remote_leader);
  if (pt.m_target == 0)
    return false; //was not able to connect to any of threads

  user_regs_struct regs;
  user_regs_struct unmap_result;
  for (auto region:regions) {
    if (verbose_level >= 4) std::cerr << "Unmapping remote mem " << (void*)region.first <<
        "-" << (void*)(region.first + region.second) << std::endl;
    pt.inject_syscall(syscall_rip,[&](user_regs_struct& regs){
      regs.rax = SYS_munmap;
      regs.rdi = region.first;
      regs.rsi = region.second;
    }, unmap_result);
    if (unmap_result.rax == -1) {
      if (verbose_level >= 1) std::cerr << "Failed unmapping remote mem " << (void*)region.first <<
          "-" << (void*)(region.first + region.second) << std::endl;
      return false;
    }
  }
  pt.detach();

  return true;
}



bool init_agent_interface(Manager& mgr, pid_t remote, bool use_agent_so, bool pause_for_ptrace)
{
  pid_t remote_leader = find_leader_thread(remote);
  std::pair<uint64_t, uint64_t> addr;
  bool addr_used = false;
  //first try to connect to existing agent
  UnixIO io_m;
  int server_fd = io_m.server(remote_leader);
  if (server_fd != -1) {
    close(server_fd);
    if (use_agent_so) {
      bool b;
      std::thread loader([&](){
      b = init_agent_so(remote, remote_leader);
      });
      loader.join();
      if (!b) {
        if (verbose_level >= 1)
          std::cerr << "Unable to use remote agent.so" << std::endl;
        return false;
      }
    } else {
      bool b;
      std::thread loader([&](){
        b = load_binary_agent(remote, remote_leader, pause_for_ptrace, addr);
      });
      loader.join();
      if (!b) {
        if (verbose_level >= 1)
          std::cerr << "Failed to load wallclock agent to remote" << std::endl;
        return false;
      }
      addr_used = true;
    }
  }
  long ret;
  int conn_fd;

  int count = 0;
  conn_fd = -1;
  while (count < 30000 && conn_fd ==-1)
  {
    conn_fd = mgr.io.connect(remote_leader);
    count++;
    usleep(100*1000);
  }
  if (conn_fd == -1) {
    if (verbose_level>=1)
      std::cout << "Failed to initialize remote agent" << std::endl;
    return false;
  }
  if (addr_used)
    mgr.set_image(addr.first, addr.second);
  if (verbose_level>=3)
    std::cout << "Remote agent initialized" << std::endl;
  return true;
}



