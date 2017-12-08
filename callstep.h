/*
 * callstep.h
 *
 *  Created on: Nov 28, 2017
 *      Author: adam
 */

#ifndef CALLSTEP_H_
#define CALLSTEP_H_
#include <map>
#include <tuple>
#include <string>

#include <string>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>


class callstep
{
public:
  uint64_t hit_count{0};
  uint64_t base_addr;
  uint64_t end_addr;
  uint64_t ip_addr;
  std::string name;
  std::map<uint64_t, callstep*> children;

  callstep(std::string name, uint64_t base_addr)
    : name(name), base_addr(base_addr)
  {
    end_addr=base_addr;
  }

  static std::pair<std::string, int64_t> get_symbol(uint64_t ip_addr);

  callstep* find_function(uint64_t ip_addr);
  void print(uint32_t depth, std::ostream& out);
};


#endif /* CALLSTEP_H_ */
