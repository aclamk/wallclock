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

#define local_assert(x) do { if(!x) *(char*)nullptr=0; } while(false)


extern "C"
void R_create_sampling_context();

extern "C"
void R_init_agent();

extern "C"
void R_print_peek(thread_sampling_ctx* sc);

extern "C"
void _remote_return(uint64_t a=0, uint64_t b=0, uint64_t c=0);


agent::agent()
{
  sem_init(&wake_up, 0, 0);
}

agent* agent::create()
{
  agent* a = new agent();
  return a;
}

void agent::add_thread(thread_sampling_ctx* sc)
{
  threads.push_back(sc);
}

agent* the_agent = nullptr;

int backtrace_reader(void* arg);


void R_init_agent()
{
  printf("R_init_agent\n");
  the_agent = agent::create();//new agent;
  pid_t v;
  constexpr size_t stack_size = 1024 * 64;
  char *vstack = (char*)malloc(stack_size);
  if (clone(agent::worker, vstack + stack_size,
                 CLONE_PARENT_SETTID | CLONE_FILES | CLONE_FS | CLONE_IO | CLONE_VM,
            //CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_THREAD|CLONE_SYSVSEM|CLONE_SETTLS|CLONE_PARENT_SETTID|CLONE_CHILD_CLEARTID,
            the_agent, &v) == -1) {
    perror("failed to spawn child task");
    _remote_return(0);
    return;
  }
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
  size_t size{4096};
  std::atomic<size_t> produced{0};
  size_t consumed{0};
  sem_t notify;
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

      if (avail < conv->size/2) {
        //make sure reader is notified
        sem_trywait(&conv->notify);
        sem_post(&conv->notify);
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
  if (sem_trywait(&conv->notify) == 0)
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
  }
}






void R_print_peek(thread_sampling_ctx* sc)
{
  _remote_return(0);
  printf("R_print_peek counter=%ld pfunc=%ld\n",sc->counter.load(), sc->pfunc.load());
  sc->root->print(0, std::cout);

}





int create_unix_server(uint64_t some_id)
{
  int serv_fd, cfd;
  struct sockaddr_un serv_addr;

  std::string unix_name{"@/wallclock/"};
  char hex[8*2+1];
  sprintf(hex, "%16.16lx", some_id);
  unix_name.append(hex);

  serv_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (serv_fd != -1)
  {
    memset(&serv_addr, 0, sizeof(struct sockaddr_un));
    serv_addr.sun_family = AF_UNIX;
    strncpy(serv_addr.sun_path, unix_name.c_str(), sizeof(serv_addr.sun_path) - 1);
    serv_addr.sun_path[0] = '\0';

    if (bind(serv_fd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr_un)) != -1)
    {
      if (listen(serv_fd, 1) == 0)
      {
        return serv_fd;
      }
    }
    else
      close(serv_fd);
  }
  return -1;
}


int wait_unix_conn(int serv_fd)
{
  int flags;
  int conn_fd = -1;
  flags = fcntl(serv_fd, F_GETFL);
  if (fcntl(serv_fd, F_SETFL, flags | O_NONBLOCK) == 0)
  {
    struct sockaddr_un peer_addr;
    socklen_t peer_addr_size;
    int i=0;
    do
    {
      conn_fd = accept(serv_fd, (struct sockaddr *) &peer_addr, &peer_addr_size);
      if (conn_fd == -1)
      {
        printf("errno=%d\n",errno);
        if (errno != EWOULDBLOCK)
          break;
        else
          usleep(100*1000);
      }
      i++;
    } while ((conn_fd == -1) && (i < 10));
  }

  fcntl(serv_fd, F_SETFL, flags);
  return conn_fd;
}




bool agent::trace_thread_new()
{
  printf("R_create_sampling_context\n");
  thread_sampling_ctx* sc = thread_sampling_ctx::create();
  printf("the_agent=%p sc=%p\n",the_agent, sc);
  the_agent->add_thread(sc);
  _remote_return((uint64_t)sc);
  printf(">>>sc=%p\n",sc);

  write_bytes(&sc,sizeof(sc));
  return true;
}


int agent::wait_read()
{
  struct pollfd fds;
  fds.fd = conn_fd;
  fds.events = POLLIN;
  int r;
  r = poll(&fds, 1, 100);
  if (r == -1)
    r = -errno;
  return r;
}

bool agent::read_bytes(void* ptr, size_t size)
{
  int res;
  res = read(conn_fd, ptr, size);
  return res == size;
}

bool agent::write_bytes(const void* ptr, size_t size)
{
  int res;
  res = write(conn_fd, ptr, size);
  return res == size;
}

bool agent::worker()
{

  uint8_t cmd;

  int r;
  bool do_continue = true;
  while (do_continue)
  {
    r = agent::wait_read();
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
      if (!read_bytes(&cmd,sizeof(uint8_t)))
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

      }
    }
  }
  return true;
}


int agent::worker(void* arg)
{
  agent* an_agent = (agent*)arg;
  printf("subthread\n");
  int server_fd;
  int conn_fd;
  server_fd = create_unix_server(111111);
  printf("server_fd=%d\n",server_fd);
  conn_fd = wait_unix_conn(server_fd);
  close(server_fd);
  printf("conn_fd = %d\n", conn_fd);
  an_agent->conn_fd = conn_fd;

  an_agent->worker();
  return 0;
}


