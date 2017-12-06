/*
 * agent.h
 *
 *  Created on: Dec 1, 2017
 *      Author: adam
 */

#ifndef AGENT_H_
#define AGENT_H_

#include <vector>
struct conveyor;
class thread_sampling_ctx
{
  thread_sampling_ctx(size_t depth);

public:
  conveyor* conv;
  /*creates sampling context for calling thread*/
  static thread_sampling_ctx* create();
  void peek();
};


class agent
{
  agent();
public:
  static agent* create();
  void add_thread(thread_sampling_ctx* sc);

//private:
  std::vector<thread_sampling_ctx*> threads;
  sem_t wake_up;
};


bool init_sampling_context_key();

extern "C"
void R_create_sampling_context();

extern "C"
void R_init_agent();

#endif /* AGENT_H_ */
