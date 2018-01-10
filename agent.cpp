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
#include <unistd.h>
#include <sys/syscall.h>
#define local_assert(x) do { if(!(x)) *(char*)nullptr=0; } while(false)

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>


extern "C"
void _init_wallclock();

extern "C"
void _init_agent();

extern "C"
void _remote_return(uint64_t a=0, uint64_t b=0, uint64_t c=0);


Agent::Agent()
{
}

Agent::~Agent()
{
  for(auto &i:threads)
  {
    ptrace_detach(i->tid);
    delete(i);
  }
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


constexpr size_t stack_size = 1024 * 64;
char vstack[stack_size];

void _init_agent()
{
  the_agent = Agent::create();//new agent;
  pid_t v;
  if (clone(Agent::worker, vstack + stack_size,
                 CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM,
            the_agent, &v) == -1) {
    _remote_return(0);
    return;
  }
  agent_pid = v;
  //todo set thread name
  _remote_return(111111);
}

void _init_wallclock()
{
  printf("INIT WALLCLOCK");
  for(int i=0;i<100;i++)
    printf("INIT %d\n",i);
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
  size_t size{4096};
  std::atomic<size_t> produced{0};
  size_t consumed{0};
  uint64_t* ip_table;
};

static constexpr uint64_t impossible_ip = 0x7fffeeeeddddccccLL;

extern "C"
void _get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, thread_sampling_ctx* sc);

void _get_backtrace(uint64_t rip, uint64_t rbp, uint64_t rsp, thread_sampling_ctx* sc)
{
  //printf("get_backtrace start \n");
  sc->backtrace_injected++;
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
      sc->backtrace_collected++;
    }
#if 0
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
#endif
    local_assert(sc->lock.exchange(false) == true);
  }
  //printf("get_backtrace end \n");
}

thread_sampling_ctx::thread_sampling_ctx(size_t size)
{
  conv = new conveyor(size);
  root = new callstep(std::string(), 0);
}

thread_sampling_ctx::~thread_sampling_ctx()
{
  delete conv;
  delete root;
}

thread_sampling_ctx* thread_sampling_ctx::create()
{
  thread_sampling_ctx* sc = new thread_sampling_ctx(16384);
  return sc;
}

void thread_sampling_ctx::consume()
{
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
}

void thread_sampling_ctx::peek()
{
  consume();
}



bool thread_sampling_ctx::dump_tree(UnixIO& io)
{
  bool res;
  res = root->dump_tree(0, io);
  uint32_t depth = 0xffffffff;
  if (res) io.write(depth);
  return res;
}



bool Agent::dump_tree(thread_sampling_ctx* tsx)
{
  tsx->dump_tree(io);
  return true;
}

bool Agent::dump_tree()
{
  pid_t tid;
  bool res = false;
  if (io.read(tid))
  {
    size_t i=0;
    for (i=0;i<threads.size();i++)
    {
      if (threads[i]->tid == tid) break;
    }
    local_assert (i != threads.size());
    thread_sampling_ctx* tsx = threads[i];
    tsx -> consume();

    //cat /proc/8805/stat
    int name;
    int f;
    int s;
    char fname[25];
    char stat[100];
    sprintf(fname, "/proc/%d/stat", tid);
    f = open(fname, O_RDONLY);
    s = read(f, stat, 99);
    close(f);
    if (s >= 0) stat[s] = 0;
    char *pb, *pe;
    pb = strchr(stat, '(');
    pe = strchr(pb, ')');
    std::string tid_name;
    if (pb && pe)
    {
      tid_name = std::string(pb + 1, pe - pb - 1);
    }
    res = io.write(tid_name);
    if (res) res = io.write(tsx->root->hit_count);
    if (res) res = io.write(tsx->time_suspended);
    if (res) res = dump_tree(tsx);
  }
  return res;
}

bool Agent::trace_attach()
{
  bool ret = false;
  pid_t pid;
  if(io.read(pid))
  {
    ret = ptrace_attach(pid);
  }
  return true;
}

bool Agent::ptrace_attach(pid_t pid)
{
  bool ret = false;
  int res;
  res = ptrace(PTRACE_SEIZE, pid, 0, 0);
  if (res == 0) {
    thread_sampling_ctx* sc = thread_sampling_ctx::create();
    sc->tid = pid;
    add_thread(sc);
    ret = true;
  }
  return ret;
}

bool Agent::ptrace_detach(pid_t pid)
{
  int res;
  res = ptrace(PTRACE_DETACH, pid, 0, 0);
  return res==0;
}

uint64_t now()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec*1000000000 + t.tv_nsec;
}

bool Agent::probe()
{
  for (size_t i = 0; i<threads.size(); i++)
  {
    int ret;
    uint64_t start = now();
    ret = ptrace(PTRACE_INTERRUPT, threads[i]->tid, 0, 0);
    pid_t wpid;
    int wstatus;
    user_regs_struct regs;
    if (ret == 0)
    {
      uint32_t sleep_time = 1;
      uint32_t iter = 1;
      do {
        wpid = waitpid(threads[i]->tid, &wstatus, WNOHANG);
        if (wpid == 0) {
          usleep(sleep_time);
          sleep_time += iter;
          iter++;
        }
      }
      while ((wpid == 0) && (iter < 100));
      if (wpid==0) {
        //printf("cannot stop thread %d\n", threads[i]->tid);
        continue;
      }
      if (WSTOPSIG(wstatus) != SIGTRAP) {
        //printf("NOT SIGTRAP %d \n",WSTOPSIG(wstatus));
        ptrace(PTRACE_CONT, threads[i]->tid, 0, 0);
        continue;
      }
      if (wpid == threads[i]->tid)
      {
        ret = ptrace(PTRACE_GETREGS, threads[i]->tid, nullptr, &regs);
        if (ret == 0)
        {
          _get_backtrace(regs.rip, regs.rbp, regs.rsp, threads[i]);
        }
        else
        {
          //printf("failed to PTRACE_GETREGS\n");
        }
      }
      ret = ptrace(PTRACE_CONT, threads[i]->tid, 0, 0);
    }
    else
    {
      //printf("unsuccessfull stop of %d\n",threads[i]->tid);
    }
    threads[i]->time_suspended += (now() - start);
  }
  return true;
}


bool Agent::worker()
{

  int server_fd;
  int conn_fd;
  server_fd = io.server(111111);
  printf("server_fd=%d\n",server_fd);
  conn_fd = io.accept(server_fd);
  close(server_fd);

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
        case CMD_TERMINATE:
          do_continue = false;
          break;

        case CMD_DUMP_TREE:
          if (!dump_tree())
            do_continue = false;
          break;
        case CMD_TRACE_ATTACH:
          if (!trace_attach())
            do_continue = false;
          break;
        case CMD_PROBE:
          probe();
          uint32_t confirm = 0;
          do_continue = io.write(confirm);
          for (auto &it:threads)
          {
            it->peek();
          }
          break;
      }
    }
  }
  close(conn_fd);
  return true;
}


int Agent::worker(void* arg)
{
  Agent* an_agent = (Agent*)arg;
  an_agent->worker();
  delete an_agent;
  syscall(SYS_exit, 0);
  return 0;
}


