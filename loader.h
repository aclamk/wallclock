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
  vm_addr_t _wc_inject;
  vm_addr_t _wc_inject_backtrace;
  vm_addr_t _wc_inject_backtrace_delayed;
  vm_addr_t R_init_agent;
  vm_addr_t R_create_sampling_context;
  vm_addr_t R_print_peek;
};

extern RemoteAPI agent_interface_remote;
bool init_agent_interface(Manager& mgr, pid_t remote);

#endif /* LOADER_H_ */
