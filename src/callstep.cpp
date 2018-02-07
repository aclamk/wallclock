/*
 * callstep.cpp
 *
 *  Created on: Nov 28, 2017
 *      Author: adam
 */

#include <tuple>
#include <string>
#include "callstep.h"
#include "agent.h"
#define UNW_LOCAL_ONLY
#include <libunwind.h>

callstep::~callstep()
{
  for(auto &i:children)
  {
    delete(i.second);
  }
}

extern Agent* the_agent;
std::pair<std::string, int64_t> callstep::get_symbol(uint64_t ip_addr)
{
  return the_agent->get_symbol(ip_addr);
  struct unw_helper
  {
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_helper()
    {
      unw_getcontext(&uc);
      uc.uc_mcontext.gregs[REG_RBP] = 0;
      uc.uc_mcontext.gregs[REG_RSP] = 0;
      unw_init_local(&cursor, &uc);
    }
  };
  static unw_helper uh;

  unw_set_reg(&uh.cursor, UNW_REG_IP, ip_addr);
  unw_word_t diff;
  size_t max_size = 20;
  int res;
  do
  {
    char buffer[max_size];
    res = unw_get_proc_name(&uh.cursor, buffer, max_size, &diff);
    if (res == -UNW_ENOMEM)
    {
      max_size = max_size*2;
      continue;
    }
    if (res == -UNW_ENOINFO)
      return std::make_pair(std::string("----"),-1);
    if (res == -UNW_EUNSPEC)
      return std::make_pair(std::string("####"),-1);
    if (res != 0)
      break;
    return std::make_pair(std::string(buffer), diff);
  } while(true);
  return std::make_pair(std::string(), -1);
}

callstep* callstep::find_function(uint64_t ip_addr)
{
  if (children.size() == 0)
  {
    std::string name;
    int64_t diff;
    std::tie(name, diff) = get_symbol(ip_addr);
    uint64_t base_addr;
    base_addr = ip_addr - diff;
    callstep* cs = new callstep(name, base_addr);
    children.emplace(base_addr, cs);
    cs->end_addr = ip_addr+1;
    cs->ip_addr = ip_addr;
    return cs;
  }

  auto ch = children.lower_bound(ip_addr+1);
  if (ch != children.begin())
  {
    ch--;
    //ch is candidate to be proper function
    if (ch->second->base_addr <= ip_addr && ip_addr < ch->second->end_addr)
    {
      //hit inside function
      return ch->second;
    }
    std::string name;
    int64_t diff;
    std::tie(name, diff) = get_symbol(ip_addr);

    uint64_t base_addr;
    base_addr = ip_addr - diff;
    if (base_addr == ch->second->base_addr)
    {
      if (ch->second->end_addr <= ip_addr)
      {
        ch->second->end_addr = ip_addr+1;
      }
      ch->second->ip_addr = ip_addr;
      return ch->second;
    }
    callstep* cs = new callstep(name, base_addr);
    cs -> end_addr = ip_addr+1;
    cs -> ip_addr = ip_addr;
    children.emplace(base_addr, cs);
    return cs;
  }
  std::string name;
  int64_t diff;
  std::tie(name, diff) = get_symbol(ip_addr);
  uint64_t base_addr;
  base_addr = ip_addr - diff;
  callstep* cs = new callstep(name, base_addr);
  cs -> end_addr = ip_addr+1;
  children.emplace(base_addr, cs);
  return cs;
}

bool callstep::dump_tree(uint32_t depth, UnixIO& io, uint32_t total_samples, double suppress)
{

  bool res;
  res = io.write(depth);
  //io.write(base_addr);
  if (res) res = io.write(name);
  if (res) res = io.write(hit_count);
  uint32_t child_count = 0;
  uint64_t last=0;
  for (auto &i: children)
  {
    if(last != i.second->base_addr)
    {
      last = i.second->base_addr;
      if (i.second->hit_count >= total_samples * suppress)
        child_count++;
    }
  }
  if (res) res = io.write(child_count);
  if (res) {
    uint64_t last=0;
    for (auto &i: children)
    {
      if(last != i.second->base_addr)
      {
        last = i.second->base_addr;
        if (i.second->hit_count >= total_samples * suppress)
          res = i.second->dump_tree(depth+1, io, total_samples, suppress);
      }
      if (!res)
        break;
    }
  }
  return res;
}



