#include <sys/ptrace.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>

#include <sys/user.h>

#include <execinfo.h>
#include <string>
#include <stdlib.h>
#include <iostream>
#include <syscall.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <atomic>
#include <semaphore.h>

#define UNW_LOCAL_ONLY
#include <libunwind.h>


#include "callstep.h"

#include "manager.h"
#include "agent.h"
#include "loader.h"


uint64_t now()
{
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec*1000000000 + t.tv_nsec;
}

template <int depth, int x> struct do_log
{
  void log(void* cct);
};

template <int x> struct do_log<8, x>
{
  void log(void* cct);
};

template<int depth, int x> void do_log<depth,x>::log(void* cct)
{
  if ((depth%2) && (rand() % 2)) {
  //if (rand() % 2) {
    do_log<depth+1, x*2> log;
    log.log(cct);
  } else {
    do_log<depth+1, x*2+1> log;
    log.log(cct);
  }
}

std::string recursion(void* cct)
{
  return "here-recursion";
}
void peek_results();
static std::atomic<uint32_t> counter{0};
//int32_t counter;
template<int x> void do_log<8, x>::log(void* cct)
{
  uint64_t i;
  //counter++;
  //i=counter;
  i=counter.fetch_add(1);
  if ((i % 10) == 0) write(0,0,0);
  if ((i % 160000) == 0) {

    std::cerr << "-------" << now() << "--------------End " << recursion(cct) << "x=" << x << " stack=" << std::hex << &i << std::dec << std::endl;
    //peek_results();

  } else {
          //std::cout << "End x=" << x << std::endl;
  }
}

int tid = 0;


















extern "C"
void _remote_return(uint64_t a, uint64_t b, uint64_t c);


bool probe(int target_pid)
{
  long ret;

  probe_thread pt;

  if (! pt.seize(target_pid))
    return false;
  sleep(1);
  user_regs_struct regs;
  int wstatus;

  if (!pt.execute_remote((interruption_func*)agent_interface_remote.R_init_agent))
    return false;
  sleep(1);
  uint64_t context;
  if (!pt.execute_remote((interruption_func*)agent_interface_remote.R_create_sampling_context, &context))
    return false;
  printf("probe 1 context=%lx\n",context);
  pt.set_remote_context(context);
  sleep(1);

  printf("R2\n");

  if (! pt.signal_interrupt())
    return false;

  for (int i=0;i<1000;i++)
  {
    if ((i%1) == 0)
    std::cout << "it " << i << std::endl;
    pid_t ppp = waitpid(target_pid, &wstatus, 0);
    if (WSTOPSIG(wstatus) == SIGTRAP)
    {
      bool b;
      b = pt.grab_callback();
      assert(b);
    }
    else
    {
      std::cout << "not SIGTRAP " << WSTOPSIG(wstatus) << std::endl;
    }
    //ret = ptrace(PTRACE_CONT, target_pid, NULL, 0);
    //assert(ret == 0);
    usleep(10*1000);
    ret = ptrace(PTRACE_INTERRUPT, target_pid, NULL, 0);
    assert(ret == 0);
  }


  waitpid(target_pid, nullptr, 0);
  pt.cont();
  if (!pt.execute_remote((interruption_func*)agent_interface_remote.R_print_peek, context))
      return false;
}












int static_tid = 0;




#define STACK_SIZE (1024 * 1024)
void* locate_library(pid_t pid, const std::string& library_name);

int main(int argc, char** argv)
{

  pid_t v;

#if 1
 //_wrapper();

  // my_backtrace(100);
  if (argc < 2)
  {
    std::cerr << "PID not provided" << std::endl;
    exit(-1);
  }
  else
  {
    v = atoi(argv[1]);
    printf("REMOTE_TID=%d\n",v);
  }

#endif

  printf("ADD=%lx\n", (uint64_t)locate_library(v,"agent.so"));

  init_agent_interface(v);
  //int pid = atoi(argv[1]);

  //std::cout << "TID=" << tid << std::endl;
  tid = v;
  //tid=pid;
  std::cout << "TID=" << tid << std::endl;
  static_tid = 0;
  probe(tid);

  long ret = ptrace (PTRACE_INTERRUPT, tid, NULL, 0);
  std::cout << "interrupt ret=" << ret << std::endl;
  waitpid(tid, nullptr, 0);



  ret =
  ptrace (PTRACE_DETACH, tid, NULL, 0);
  std::cout << "detach ret=" << ret << std::endl;

  //std::cout << "backtrace_counter=" << backtrace_counter.load() << std::endl;
  //kill(tid, 15);
  sleep(10000);
}
