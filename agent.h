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
struct callstep;

class thread_sampling_ctx
{
  thread_sampling_ctx(size_t depth);

public:
  conveyor* conv{nullptr};
  callstep* root{nullptr};
  std::atomic<uint64_t> counter{0};
  std::atomic<uint64_t> pfunc{0};
  std::atomic<bool> lock{0};
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


//bool init_sampling_context_key();

extern "C"
void R_create_sampling_context();

extern "C"
void R_init_agent();

extern "C"
void R_print_peek(thread_sampling_ctx* sc);


#endif /* AGENT_H_ */
