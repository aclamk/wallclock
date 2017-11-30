/*
 * callstep.cpp
 *
 *  Created on: Nov 28, 2017
 *      Author: adam
 */

#include <tuple>
#include <string>
#include "callstep.h"

#define UNW_LOCAL_ONLY
#include <libunwind.h>

std::pair<std::string, int64_t> callstep::get_symbol(uint64_t ip_addr)
{
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
  size_t max_size = 100;
  int res;
  do
  {
    char buffer[max_size];
    res = unw_get_proc_name(&uh.cursor, buffer, max_size, &diff);
    if (res == UNW_ENOMEM)
    {
      max_size = max_size*2;
      continue;
    }
    if (res != 0)
      break;
    return std::make_pair(std::string(buffer), diff);
  } while(false);
  return std::make_pair(std::string(), -1);
}

callstep* callstep::find_function(uint64_t ip_addr)
{
  auto ch = children.find(ip_addr);
  if (ch != children.end())
  {
    return ch->second;
  }
  else
  {
    std::string name;
    int64_t diff;
    std::tie(name, diff) = get_symbol(ip_addr);
    uint64_t base_addr = ip_addr - diff;

    auto it = children.lower_bound(base_addr);
    if ((it != children.end()) && (it->second->base_addr == base_addr))
    {
      //this is call to same function from other place
      children.emplace(ip_addr, it->second);
      return it->second;
    }
    else
    {
      //function that was never visited
      //printf("%lx name=%s\n",base_addr, name.c_str());
      callstep* cs = new callstep(name, base_addr);
      children.emplace(ip_addr, cs);
      return cs;
    }
  }
}

void callstep::print(uint32_t depth, std::ostream& out)
{

  //out << std::string(depth*2, ' ');
  //out << "hit = " << hit_count << ",\n";

  out << std::string(depth*2, ' ');
  out << std::hex << base_addr << std::dec << " " << name << " " << hit_count << "\n";

//  out << std::string(depth*2, ' ');
//  out << "addr = 0x" << std::hex << base_addr << std::dec << ",\n";

  //out << std::string(depth*2, ' ');
  //out << "{\n";
  uint64_t last=0;
  for (auto &i: children)
  {
    if(last != i.second->base_addr)
    {
      last = i.second->base_addr;
      i.second->print(depth+1, out);
    }

  }
  //out << std::string(depth*2, ' ');
  //out << "}\n";
}

