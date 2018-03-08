/*
 * loader.h
 *
 *  Created on: Dec 7, 2017
 *      Author: adam
 */

#ifndef LOADER_H_
#define LOADER_H_

#include <inttypes.h>
class Manager;
typedef uint64_t vm_addr_t;

struct RemoteAPI
{
  char sanity_marker[8];
  vm_addr_t _init_agent;
  vm_addr_t _wc_inject;
};

extern RemoteAPI agent_interface_remote;
bool init_agent_interface(Manager& mgr, pid_t remote, bool use_agent_so, bool pause_for_ptrace);

#endif /* LOADER_H_ */
