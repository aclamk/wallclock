/*
 * agent.cpp
 *
 *  Created on: Nov 16, 2017
 *      Author: adam
 */
#include <execinfo.h>
#include <sys/types.h>
#include <atomic>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <semaphore.h>
#include "agent.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <vector>
#include "callstep.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <fcntl.h>

#include <poll.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#define local_assert(x) do { if(!x) *(char*)nullptr=0; } while(false)


extern "C"
void R_create_sampling_context();

extern "C"
void R_init_agent();

extern "C"
void R_print_peek(thread_sampling_ctx* sc);

extern "C"
void _remote_return(uint64_t a=0, uint64_t b=0, uint64_t c=0);


Agent::Agent()
{
  sem_init(&wake_up, 0, 0);
}

Agent* Agent::create()
{
  Agent* a = new Agent();
  return a;
}

void Agent::add_thread(thread_sampling_ctx* sc)
{
  threads.push_back(sc);
}

Agent* the_agent = nullptr;
pid_t agent_pid = -1;
int backtrace_reader(void* arg);


void R_init_agent()
{
  printf("R_init_agent\n");
  the_agent = Agent::create();//new agent;
  pid_t v;
  constexpr size_t stack_size = 1024 * 64;
  char *vstack = (char*)malloc(stack_size);
  if (clone(Agent::worker, vstack + stack_size,
                 CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM,
            //CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID,
            the_agent, &v) == -1) {
    perror("failed to spawn child task");
    _remote_return(0);
    return;
  }
  agent_pid = v;
  //todo set thread name
  _remote_return(111111);

}

void R_create_sampling_context()
{
  printf("R_create_sampling_context\n");
  thread_sampling_ctx* sc = thread_sampling_ctx::create();
  printf("the_agent=%p sc=%p\n",the_agent, sc);
  the_agent->add_thread(sc);
  _remote_return((uint64_t)sc);
  printf(">>>sc=%p\n",sc);
}





extern "C" void grab_callstack(void);

struct conveyor
{
public:
  conveyor(size_t size) : size(size)
  {
    ip_table = new uint64_t[size];
  }

  inline uint64_t* produce_pos()
  {
    return ip_table + (produced.load() % size);
  }
  inline size_t produce_avail()
  {
    return size - (produced.load() - consumed);
  }

  inline uint64_t* consume_pos()
  {
    return ip_table + (consumed % size);
  }
  inline size_t consume_avail()
  {
    return produced.load() - consumed;
  }

  inline uint64_t* next(uint64_t* pos)
  {
    pos++;
    if (pos == ip_table + size)
      pos = ip_table;
    return pos;
  }

  inline uint64_t* prev(uint64_t* pos)
  {
    if (pos == ip_table)
      pos = ip_table + (size - 1);
    else
      pos--;
    return pos;
  }

  void inline advance(size_t how_much)
  {
    produced += how_much;
  }

//private:
public:
  enum {
    notified_none = 0,
    notified_signalled = 1,
    notified_accepted = 3,

  };
  size_t size{4096};
  std::atomic<size_t> produced{0};
  size_t consumed{0};
  std::atomic<uint8_t> notified{notified_none};
  uint64_t* ip_table;
};


static constexpr uint64_t impossible_ip = 0x7fffeeeeddddccccLL;




extern "C"
void _get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, thread_sampling_ctx* sc);


#if 0
void _get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, thread_sampling_ctx* sc)
{
  //printf(":\n");
  if (sc->lock.exchange(true) == true)
    return;
  //sampling_context* sc;
  sc->counter++;
  bool b;
  conveyor* conv = sc->conv;
  //backtrace_counter++;
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  unw_getcontext(&uc);
  uc.uc_mcontext.gregs[REG_RBP] = rbp;
  uc.uc_mcontext.gregs[REG_RIP] = rip;
  uc.uc_mcontext.gregs[REG_RSP] = rsp;
  if (rip - (uint64_t)&rand <16)
    {
      printf("\n----- close enough %lx\n",rip);
    }
  size_t count = 0;
  uint64_t* pos = conv->produce_pos();
  size_t avail = conv->produce_avail();
  if (avail <= 1) {
    goto end;
  }
  *pos = rip;
  ip = rip;
  if(*pos == 0) sc->counter--;
  pos = conv->next(pos);
  count++;
  avail--;
  if(ip == 0) goto ended;
  unw_init_local(&cursor, &uc);
  b=!(rip&0x03f);
  uint64_t rbp_arch;
  uint64_t rsp_arch;
  while (unw_step(&cursor) > 0)
  {
    unw_get_reg(&cursor, UNW_REG_SP, &rsp_arch);

    if (avail == 0) {
      goto end;
    }
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
#if 0
    if (ip>0x800000000000L){
      printf("#%ld XXXXX = %lx SP=%lx rsp=%lx rip=%lx rand=%lx\n", count, ip,rsp_arch,rsp,rip, &rand);
      printf("XXXXX rsp=%lx rbp=%lx *sp=%lx *(rbp+8)=%lx\n", rsp, rbp, *(uint64_t*)(rsp), *(uint64_t*)(rbp+8));
      //unw_set_reg(&cursor, UNW_REG_IP, ip>>8);
//      b=true;
      {
        unw_cursor_t cursor1;
        unw_init_local(&cursor1, &uc);
        while (unw_step(&cursor1) > 0)
        {
          unw_word_t ip1,sp1;
          unw_get_reg(&cursor1, UNW_REG_IP, &ip1);
          unw_get_reg(&cursor1, UNW_REG_SP, &sp1);
          printf(">>>> sp=%lx ip=%lx\n",sp1, ip1);
        }
      }
    }
#endif
    *pos = ip;
    if(*pos == 0) sc->counter--;

    pos = conv->next(pos);
    count++;
    avail--;
    if (ip == 0) goto ended;
  }
  if (ip != 0)
  {
    if(avail == 0)
      goto end;
    *pos = 0;
    if(*pos == 0) sc->counter--;

    pos = conv->next(pos);
    count++;
    avail--;
  }

  ended:
  conv->advance(count);
  if (avail < conv->size/2) {
    //make sure reader is notified
    sem_trywait(&conv->notify);
    sem_post(&conv->notify);
  }
  end:
  local_assert(sc->lock.exchange(false) == true);
}
#endif


#if 1
void _get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, thread_sampling_ctx* sc)
{
  if (sc->lock.exchange(true) == false)
  {
    bool b;
    conveyor* conv = sc->conv;
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, sp;
    unw_getcontext(&uc);
    uc.uc_mcontext.gregs[REG_RBP] = rbp;
    uc.uc_mcontext.gregs[REG_RIP] = rip;
    uc.uc_mcontext.gregs[REG_RSP] = rsp;
    size_t count = 0;
    uint64_t* pos = conv->produce_pos();
    ssize_t avail = conv->produce_avail();
    if (avail > 0) {
      *pos = rip;
      ip = rip;
      pos = conv->next(pos);
      count++;
      avail--;
    }
    unw_init_local(&cursor, &uc);
    while (avail > 0 && ip != 0 && unw_step(&cursor) > 0)
    {
      unw_get_reg(&cursor, UNW_REG_IP, &ip);
      if (ip != 0)
      {
        *pos = ip;
        pos = conv->next(pos);
        count++;
        avail--;
      }
    }
    if(avail > 0) {
      *pos = impossible_ip;
      pos = conv->next(pos);
      count++;
      avail--;

      conv->advance(count);
      sc->counter++;
#if 0
      if (avail < conv->size/2) {
        //make sure reader is notified
        uint8_t notif;
        notif = conv->notified.exchange(conv->notified_signalled);
        if (notif == conv->notified_none)
        {
          //transition from not-signalled to signalled
          syscall(SYS_tkill, agent_pid, SIGUSR1);
        }
        //sem_trywait(&conv->notify);
        //sem_post(&conv->notify);
        //tkill(agent_pid, SIGUSR1);
        //tid =
        //syscall(SYS_tkill, agent_pid, SIGUSR1);
      }
#endif
    }
    if (conv->produce_avail() < conv->size/2)
    {
      //make sure reader is notified
      uint8_t notif;
      notif = conv->notified.exchange(conv->notified_signalled);
      if (notif == conv->notified_none)
      {
        //transition from not-signalled to signalled
        syscall(SYS_tkill, agent_pid, SIGUSR1);
      }
    }

    local_assert(sc->lock.exchange(false) == true);
  }
}
#endif



thread_sampling_ctx::thread_sampling_ctx(size_t size)
{
  conv = new conveyor(size);
  root = new callstep(std::string(), 0);
}


thread_sampling_ctx* thread_sampling_ctx::create()
{
  thread_sampling_ctx* sc = new thread_sampling_ctx(4096);
  return sc;
}




//callstep* root = nullptr;
void thread_sampling_ctx::peek()
{
  printf("PEEK this=%p\n", this);
  if (conv->notified_signalled == conv->notified.exchange(conv->notified_signalled))
//  if (sem_trywait(&conv->notify) == 0)
  {
    //notified.

    uint64_t* pos = conv->consume_pos();
    ssize_t avail = conv->consume_avail();
    ssize_t avail_copy = avail;

    while (avail > 0)
    {
      pfunc++;
      uint64_t* pos_i = pos;
      size_t count = 0;
      while (*pos_i != impossible_ip)
      {
        pos_i = conv->next(pos_i);
        count++;
        avail--;
      }
      pos = pos_i;
      local_assert(*pos == impossible_ip);
      pos_i = conv->prev(pos_i);
      callstep* node = root;
      while (count > 0)
      {
        if(node)node->hit_count++;
        if(node)node = node->find_function(*pos_i);
        pos_i = conv->prev(pos_i);
        count --;
      }
      if(node)
        node->hit_count++;
      pos = conv->next(pos);
      avail--;

    }
    conv->consumed += avail_copy;
    local_assert(conv->notified_signalled == conv->notified.exchange(conv->notified_none));
  }
}



bool thread_sampling_ctx::dump_tree(UnixIO& io)
{
  bool res;
  res = root->dump_tree(0, io);
  uint32_t depth = 0xffffffff;
  if (res) io.write(depth);
  return res;
}



void R_print_peek(thread_sampling_ctx* sc)
{
  _remote_return(0);
  printf("R_print_peek counter=%ld pfunc=%ld\n",sc->counter.load(), sc->pfunc.load());
  sc->root->print(0, std::cout);

}





bool Agent::trace_thread_new()
{
  printf("R_create_sampling_context\n");
  thread_sampling_ctx* sc = thread_sampling_ctx::create();
  printf("the_agent=%p sc=%p\n",the_agent, sc);
  the_agent->add_thread(sc);
  _remote_return((uint64_t)sc);
  printf(">>>sc=%p\n",sc);

  io.write_bytes(&sc,sizeof(sc));
  return true;
}

bool Agent::dump_tree(thread_sampling_ctx* tsx)
{
  tsx->dump_tree(io);
  return true;
}

bool Agent::dump_tree()
{
  uint64_t tsx;
  bool res = false;
  if (io.read_bytes(&tsx, sizeof(uint64_t)))
  {
    res = dump_tree((thread_sampling_ctx*)tsx);
  }
  return res;
}



bool Agent::worker()
{

  int server_fd;
  int conn_fd;
  server_fd = io.server(111111);
  printf("server_fd=%d\n",server_fd);
  conn_fd = io.accept(server_fd);
  close(server_fd);
  printf("conn_fd = %d\n", conn_fd);
  //io->conn_fd = conn_fd;


  int r;
  bool do_continue = true;
  while (do_continue)
  {
    r = io.wait_read();
    if (r == -EINTR)
    {
      //scan through
      for (auto &it:threads)
      {
        it->peek();
      }
    }
    if (r>0)
    {
      uint8_t cmd;
      if (!io.read_bytes(&cmd,sizeof(uint8_t)))
      {
        do_continue = false;
        break;
      }
      switch(cmd)
      {
        case CMD_TRACE_THREAD_NEW:
          if (!trace_thread_new())
            do_continue = false;
          break;

        case CMD_TERMINATE:
          do_continue = false;
          break;

        case CMD_DUMP_TREE:
          if (!dump_tree())
            do_continue = false;
          break;
      }
    }
  }
  printf("Agent exited\n");
  return true;
}


void empty_signal(int)
{
}


int Agent::worker(void* arg)
{
  Agent* an_agent = (Agent*)arg;
  printf("subthread\n");
  signal(SIGUSR1, empty_signal);
  an_agent->worker();
  return 0;
}


