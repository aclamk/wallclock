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


uint64_t R_init_agent_call = 0;
uint64_t R_create_sampling_context_call = 0;
uint64_t R_print_peek_call = 0;

uint64_t _wrapper_call = 0;
uint64_t _wrapper_regs_provided_call = 0;
uint64_t _wrapper_to_func_call = 0;


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
    std::cout << addr_begin << addr_end << perms << offset << dev << inode << "<>" << pathname << std::endl;
    if (pathname.find(library_name)!=pathname.npos && perms == "r-xp")
    {
      return (void*)strtoll(addr_begin.c_str(), nullptr, 16);
    }
  }
  return nullptr;
}
