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

extern RemoteAPI agent_interface;

bool init_agent_interface(Manager& mgr, pid_t remote)
{
  void* my_agent_so = locate_library(syscall(SYS_gettid), "agent.so");
  void* remote_agent_so = locate_library(remote, "agent.so");
  assert(my_agent_so != nullptr);
  if (remote_agent_so == nullptr) {
    std::cout << "remote does not have agent.so library" << std::endl;
    exit(-1);
  }
  assert(memcmp(agent_interface.sanity_marker,"AGENTAPI",8) == 0);

  int64_t diff = (uint64_t)remote_agent_so - (uint64_t)my_agent_so;

  agent_interface_remote = agent_interface;
  agent_interface_remote._init_agent += diff;
  agent_interface_remote._wc_inject += diff;

  long ret;
  monitored_thread pt;
  int conn_fd;
  if (! pt.seize(remote))
  {
    std::cerr << "Target " << remote << " cannot be sized" << std::endl;
    return false;
  }
  user_regs_struct regs;
  int wstatus;
  uint64_t unix_id;
  if (!pt.execute_remote((interruption_func*)agent_interface_remote._init_agent, &unix_id))
  {
    std::cerr << "failed to execute remote agent" << std::endl;
    return false;
  }
  int count = 0;
  conn_fd = -1;
  while (count < 30 && conn_fd ==-1)
  {
    conn_fd = mgr.io.connect(unix_id);
    count++;
    usleep(100*1000);
  }
  if (conn_fd == -1)
    return false;

  if (!pt.detach())
    return false;
  return true;
}



