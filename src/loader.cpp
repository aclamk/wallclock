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
#include <dirent.h>
#include <thread>
#include <dlfcn.h>

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

std::vector<pid_t> list_threads_in_group(pid_t pid)
{
  std::vector<pid_t> threads;
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
        threads.push_back(tid);
      }
    }
    closedir (dir);
  }
  return threads;
}

bool init_agent_so(pid_t remote, pid_t remote_leader)
{
  void* handle = dlopen("libagent.so",RTLD_NOW);
  if (!handle) {
    std::cout << "unable to load libagent.so" << std::endl;
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
    std::cout << "remote does not have agent.so library" << std::endl;
    return false;
  }
  if (memcmp(agent_interface->sanity_marker,"AGENTAPI",8) != 0) {
    return false;
  }

  int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;
  agent_interface_remote = *agent_interface;
  agent_interface_remote._init_agent += diff;
  agent_interface_remote._wc_inject += diff;
  std::vector<pid_t> threads_in_group = list_threads_in_group(remote_leader);
  monitored_thread pt;
  size_t i;
  for(i = 0; i < threads_in_group.size(); i++) {
    if (pt.attach(threads_in_group[i])) {
      if (pt.pause_outside_syscall()) {
        break;
      } else {
        pt.detach();
      }
    }
  }
  if (i == threads_in_group.size()) {
    std::cerr << "Target " << remote << " cannot be sized" << std::endl;
    return false;
  }
  if (!pt.execute_remote((interruption_func*)agent_interface_remote._init_agent,
                         (uint64_t)remote_leader)) {
    std::cerr << "failed to execute remote agent" << std::endl;
    return false;
  }
  return true;
}


monitored_thread pause_outside_syscall(pid_t remote_leader)
{
  std::vector<pid_t> threads_in_group = list_threads_in_group(remote_leader);

  std::vector<monitored_thread> mt;
  for (size_t i = 0; i < threads_in_group.size(); i++) {
    monitored_thread pt;
    if (pt.seize(threads_in_group[i])) {
      mt.push_back(pt);
    }
  }

  for (auto i = mt.begin(); i != mt.end(); ) {
    if ((*i).signal_interrupt()) {
      int wstatus;
      if ((*i).wait_status(&wstatus, 1000)) {
        if ((*i).single_step()) {
          i++;
          continue;
        }
      }
    }
    (*i).detach();
    i = mt.erase(i);
  }
  monitored_thread pt;
  for (auto i = mt.begin(); i != mt.end(); ) {
    int wstatus;
    if ((*i).wait_status(&wstatus, 1000)) {
      if ((*i).in_syscall()) {
        pt = *i;
        i = mt.erase(i);
        break;
      }
    }
  }
  for (auto i = mt.begin(); i != mt.end(); ) {
    (*i).detach();
    i = mt.erase(i);
  }

  return pt;
}


bool load_binary_agent(pid_t remote, pid_t remote_leader, bool pause_for_ptrace)
{
  uint64_t syscall_rip;
  std::vector<pid_t> threads_in_group = list_threads_in_group(remote_leader);
  size_t i;
  for(i = 0; i < threads_in_group.size(); i++)
  {
    monitored_thread pt;
    if (pt.seize(threads_in_group[i])) {
      pt.locate_syscall(&syscall_rip);
      pt.detach();
      break;
    }
  }
  if (i == threads_in_group.size())
    return false; //was not able to locate place of rip


  monitored_thread pt = pause_outside_syscall(remote_leader);
  if (pt.m_target == 0)
    return false; //was not able to connect to any of threads

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
  if (mmap_result.rax == -1) return false;
  char* remote_image = (char*) mmap_result.rax;
  struct iovec local_iov;
  struct iovec remote_iov;
  local_iov.iov_base = local_image;
  local_iov.iov_len = buf.st_size;
  remote_iov.iov_base = remote_image;
  remote_iov.iov_len = buf.st_size;

  res = process_vm_writev(remote, &local_iov, 1, &remote_iov, 1, 0);
  pt.execute_init((uint64_t)remote_image, pause_for_ptrace?remote_leader|0x100000000:remote_leader);
  pt.wait_return();
  pt.detach();

  return true;
}



bool init_agent_interface(Manager& mgr, pid_t remote, bool use_agent_so, bool pause_for_ptrace)
{
  pid_t remote_leader = find_leader_thread(remote);

  //first try to connect to existing agent
  UnixIO io_m;
  int server_fd = io_m.server(remote_leader);
  printf("check_server_fd=%d\n",server_fd);
  if (server_fd != -1) {
    close(server_fd);
    if (use_agent_so) {
      bool b;
      std::thread loader([&](){
      b = init_agent_so(remote, remote_leader);
      });
      loader.join();
      if (!b) {
        std::cerr << "Unable to use remote agent.so" << std::endl;
        return false;
      }
    } else {
      bool b;
      std::thread loader([&](){
        b = load_binary_agent(remote, remote_leader, pause_for_ptrace);
      });
      loader.join();
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
  while (count < 30000 && conn_fd ==-1)
  {
    conn_fd = mgr.io.connect(remote_leader);
    count++;
    usleep(100*1000);
  }
  if (conn_fd == -1)
    return false;

  return true;
}



